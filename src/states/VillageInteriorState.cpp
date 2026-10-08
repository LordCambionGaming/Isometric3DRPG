#include "states/VillageInteriorState.h"
#include "states/InventoryState.h"

#include "core/Game.h"
#include "core/KeyBindings.h"
#include "core/Localization.h"
#include "entities/Player.h"
#include "rendering/Camera3DController.h"
#include "states/VillageInteractionState.h"
#include "world/Village.h"
#include "ui/GothicTheme.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <queue>
#include <raylib.h>
#include <raymath.h>

namespace {

constexpr int kInteriorGridSize = 25;
constexpr float kInteriorGridStep = 0.35f;
constexpr float kInteriorGridMinimum = -4.2f;
constexpr int kInteriorCellCount = kInteriorGridSize * kInteriorGridSize;

bool HitsInteriorFurniture(int buildingIndex, float x, float z) {
    const Vector2 point{ x, z };
    if (buildingIndex == 0) {
        return CheckCollisionPointRec(point, { -4.0f, -2.65f, 2.0f, 1.3f }) ||
               CheckCollisionPointRec(point, { -0.2f, -0.5f, 2.4f, 1.0f });
    }
    if (buildingIndex == 1)
        return CheckCollisionPointRec(point, { -1.9f, -2.23f, 3.8f, 1.25f });
    if (buildingIndex == 2)
        return CheckCollisionPointRec(point, { -1.3f, -3.2f, 2.6f, 1.0f });
    return CheckCollisionPointRec(point, { -3.3f, -3.4f, 6.6f, 0.8f }) ||
           CheckCollisionPointRec(point, { -3.45f, -0.35f, 1.5f, 0.7f }) ||
           CheckCollisionPointRec(point, { 1.95f, -0.35f, 1.5f, 0.7f });
}

Vector2 InteriorGridPosition(int cellX, int cellZ) {
    return { kInteriorGridMinimum + cellX * kInteriorGridStep,
             kInteriorGridMinimum + cellZ * kInteriorGridStep };
}

int InteriorCellIndex(int cellX, int cellZ) {
    return cellZ * kInteriorGridSize + cellX;
}

Vector2 InteriorCellForPosition(float x, float z) {
    return {
        static_cast<float>(std::clamp(static_cast<int>(std::round((x - kInteriorGridMinimum) /
            kInteriorGridStep)), 0, kInteriorGridSize - 1)),
        static_cast<float>(std::clamp(static_cast<int>(std::round((z - kInteriorGridMinimum) /
            kInteriorGridStep)), 0, kInteriorGridSize - 1))
    };
}

bool IsInteriorCellWalkable(int buildingIndex, int cellX, int cellZ) {
    const Vector2 center = InteriorGridPosition(cellX, cellZ);
    constexpr float bodyRadius = 0.24f;
    const std::array<Vector2, 5> footprint = {{
        { 0.0f, 0.0f }, { -bodyRadius, 0.0f }, { bodyRadius, 0.0f },
        { 0.0f, -bodyRadius }, { 0.0f, bodyRadius }
    }};
    for (const Vector2 offset : footprint) {
        const float x = center.x + offset.x;
        const float z = center.y + offset.y;
        if (HitsInteriorFurniture(buildingIndex, x, z)) return false;
    }
    return true;
}

}

VillageInteriorState::VillageInteriorState(Game* game, int buildingIndex)
    : GameState(game), m_buildingIndex(buildingIndex) {
    m_type = GameStateType::VillageInterior;
}

void VillageInteriorState::Enter() {
    m_path.clear();
    m_pathIndex = 0;
    m_moving = false;
}
void VillageInteriorState::Exit() {}

void VillageInteriorState::SetMoveTarget(float x, float z) {
    Player* player = m_game->GetPlayer();
    if (!player) return;

    const Vector2 startCell = InteriorCellForPosition(player->GetPosition().x, player->GetPosition().z);
    const Vector2 targetCell = InteriorCellForPosition(x, z);
    const int startX = static_cast<int>(startCell.x);
    const int startZ = static_cast<int>(startCell.y);
    const int targetX = static_cast<int>(targetCell.x);
    const int targetZ = static_cast<int>(targetCell.y);
    if (!IsInteriorCellWalkable(m_buildingIndex, targetX, targetZ)) return;

    std::array<int, kInteriorCellCount> parents;
    parents.fill(-1);
    std::queue<int> open;
    const int startIndex = InteriorCellIndex(startX, startZ);
    const int targetIndex = InteriorCellIndex(targetX, targetZ);
    parents[static_cast<std::size_t>(startIndex)] = startIndex;
    open.push(startIndex);
    constexpr std::array<Vector2, 4> directions = {{
        { 1.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, -1.0f }
    }};

    while (!open.empty() && parents[static_cast<std::size_t>(targetIndex)] < 0) {
        const int current = open.front();
        open.pop();
        const int currentX = current % kInteriorGridSize;
        const int currentZ = current / kInteriorGridSize;
        for (const Vector2 direction : directions) {
            const int nextX = currentX + static_cast<int>(direction.x);
            const int nextZ = currentZ + static_cast<int>(direction.y);
            if (nextX < 0 || nextX >= kInteriorGridSize || nextZ < 0 || nextZ >= kInteriorGridSize ||
                !IsInteriorCellWalkable(m_buildingIndex, nextX, nextZ)) continue;
            const int next = InteriorCellIndex(nextX, nextZ);
            if (parents[static_cast<std::size_t>(next)] >= 0) continue;
            parents[static_cast<std::size_t>(next)] = current;
            open.push(next);
        }
    }
    if (parents[static_cast<std::size_t>(targetIndex)] < 0) return;

    std::vector<int> reversedPath;
    for (int current = targetIndex; current != startIndex;
         current = parents[static_cast<std::size_t>(current)]) {
        reversedPath.push_back(current);
    }
    std::reverse(reversedPath.begin(), reversedPath.end());

    m_path.clear();
    m_path.reserve(reversedPath.size());
    for (const int cell : reversedPath) {
        const Vector2 position = InteriorGridPosition(cell % kInteriorGridSize, cell / kInteriorGridSize);
        m_path.push_back({ position.x, 0.0f, position.y });
    }
    m_pathIndex = 0;
    m_moving = !m_path.empty();
    if (!m_moving) player->MoveBy({}, GetFrameTime());
}

void VillageInteriorState::Update(float deltaTime) {
    Player* player = m_game->GetPlayer();
    if (!player) return;
    if (!m_moving || m_pathIndex >= m_path.size()) {
        if (!m_moving) player->MoveBy({}, deltaTime);
        return;
    }

    const Vector3 before = player->GetPosition();
    const Vector3 waypoint = m_path[m_pathIndex];
    const float deltaX = waypoint.x - before.x;
    const float deltaZ = waypoint.z - before.z;
    const float distance = std::sqrt(deltaX * deltaX + deltaZ * deltaZ);
    const float step = 3.2f * std::min(deltaTime, 0.05f);
    const float ratio = distance > 0.001f ? std::min(1.0f, step / distance) : 1.0f;
    const float nextX = before.x + deltaX * ratio;
    const float nextZ = before.z + deltaZ * ratio;

    if (std::fabs(nextX) <= 0.42f && before.z < 3.95f && nextZ >= 3.95f) {
        m_game->ExitVillageBuilding();
        return;
    }
    player->MoveBy({ nextX - before.x, 0.0f, nextZ - before.z }, deltaTime);
    if (Camera3DController* camera = m_game->GetCamera()) {
        camera->FollowTarget(player->GetPosition());
    }
    if (distance <= step + 0.001f) ++m_pathIndex;
    if (m_pathIndex >= m_path.size()) {
        m_path.clear();
        m_pathIndex = 0;
        m_moving = false;
        player->MoveBy({}, deltaTime);
    }
}

void VillageInteriorState::Render() {
    const Font& font = ui::theme::UIFont();
    const std::string title = lang::Get(Village::BuildingNameKey(m_buildingIndex));
    const std::string hint = lang::Get("village.interior.exit_hint");
    const std::string movementHint = lang::Get("village.interior.move_hint");
    const Rectangle button = { GetScreenWidth() * 0.5f - 126.0f, GetScreenHeight() - 76.0f, 252.0f, 48.0f };
    DrawRectangleRounded(button, 0.08f, 6, Color{ 18, 15, 15, 230 });
    DrawRectangleRoundedLines(button, 0.08f, 6, ui::theme::kGoldDim);
    ui::theme::DrawTextCentered(font, title.c_str(), GetScreenWidth() * 0.5f, 18.0f, 24.0f, ui::theme::kGoldBright);
    ui::theme::DrawTextCentered(font, movementHint.c_str(), GetScreenWidth() * 0.5f, button.y - 30.0f,
                                18.0f, ui::theme::kBone);
    ui::theme::DrawTextCentered(font, hint.c_str(), GetScreenWidth() * 0.5f, button.y + 14.0f, 18.0f, ui::theme::kBone);

    if (Village* village = m_game->GetVillage()) {
        if (Camera3DController* camera = m_game->GetCamera()) {
            VillageNpc hoveredNpc;
            const Ray ray = GetMouseRay(GetMousePosition(), camera->GetCamera());
            if (village->NpcAtInteriorRay(ray, m_buildingIndex, hoveredNpc)) {
                const Vector3 anchor{ hoveredNpc.position.x, hoveredNpc.position.y + 2.0f,
                                      hoveredNpc.position.z };
                const Vector2 screen = GetWorldToScreen(anchor, camera->GetCamera());
                const std::string name = lang::Get(hoveredNpc.id);
                const Vector2 size = MeasureTextEx(font, name.c_str(), 22.0f, 1.5f);
                const Rectangle label = { screen.x - size.x * 0.5f - 10.0f, screen.y - 34.0f,
                                           size.x + 20.0f, 30.0f };
                DrawRectangleRounded(label, 0.12f, 5, Color{ 18, 15, 15, 235 });
                DrawRectangleRoundedLines(label, 0.12f, 5, ui::theme::kGoldDim);
                DrawTextEx(font, name.c_str(), { label.x + 10.0f, label.y + 4.0f },
                           22.0f, 1.5f, ui::theme::kGoldBright);
            } else if (InteriorChest* hoveredChest = village->ChestAtInteriorRay(ray, m_buildingIndex)) {
                const Vector3 anchor{ hoveredChest->position.x, hoveredChest->position.y + 1.0f,
                                      hoveredChest->position.z };
                const Vector2 screen = GetWorldToScreen(anchor, camera->GetCamera());
                const std::string name = hoveredChest->looted ? "Cassa (Vuota)" : "Cassa (Saccheggia)";
                const Vector2 size = MeasureTextEx(font, name.c_str(), 20.0f, 1.5f);
                const Rectangle label = { screen.x - size.x * 0.5f - 8.0f, screen.y - 30.0f,
                                           size.x + 16.0f, 26.0f };
                DrawRectangleRounded(label, 0.12f, 5, Color{ 18, 15, 15, 235 });
                DrawRectangleRoundedLines(label, 0.12f, 5, hoveredChest->looted ? ui::theme::kBone : ui::theme::kGoldBright);
                DrawTextEx(font, name.c_str(), { label.x + 8.0f, label.y + 3.0f },
                           20.0f, 1.5f, hoveredChest->looted ? ui::theme::kBone : ui::theme::kGoldBright);
            }
        }
    }
}

void VillageInteriorState::HandleInput() {
    const Rectangle exitButton = { GetScreenWidth() * 0.5f - 126.0f, GetScreenHeight() - 76.0f, 252.0f, 48.0f };
    if (core::KeyBindings::Pressed(core::KeyBindings::Action::Back) ||
        (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), exitButton))) {
        m_game->ExitVillageBuilding();
        return;
    }

    Player* player = m_game->GetPlayer();
    Camera3DController* camera = m_game->GetCamera();
    if (!player || !camera || !IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) return;

    const Ray ray = GetMouseRay(GetMousePosition(), camera->GetCamera());
    VillageNpc clickedNpc;
    if (Village* village = m_game->GetVillage();
        village && village->NpcAtInteriorRay(ray, m_buildingIndex, clickedNpc)) {
        const Vector3 playerPosition = player->GetPosition();
        const float dx = playerPosition.x - clickedNpc.position.x;
        const float dz = playerPosition.z - clickedNpc.position.z;
        const float distance = std::sqrt(dx * dx + dz * dz);
        if (distance <= 2.2f) {
            m_path.clear();
            m_pathIndex = 0;
            m_moving = false;
            m_game->SetState(std::make_unique<VillageInteractionState>(m_game, clickedNpc, m_buildingIndex));
        } else {
            const float inverseDistance = distance > 0.001f ? 1.0f / distance : 0.0f;
            SetMoveTarget(clickedNpc.position.x + dx * inverseDistance * 1.05f,
                          clickedNpc.position.z + dz * inverseDistance * 1.05f);
        }
        return;
    }

    // Check chest interaction
    if (Village* village = m_game->GetVillage()) {
        if (InteriorChest* chest = village->ChestAtInteriorRay(ray, m_buildingIndex)) {
            const Vector3 playerPosition = player->GetPosition();
            const float dx = playerPosition.x - chest->position.x;
            const float dz = playerPosition.z - chest->position.z;
            const float distance = std::sqrt(dx * dx + dz * dz);
            if (distance <= 2.2f) {
                m_path.clear();
                m_pathIndex = 0;
                m_moving = false;
                
                m_game->SetState(std::make_unique<InventoryState>(m_game, chest));
            } else {
                const float inverseDistance = distance > 0.001f ? 1.0f / distance : 0.0f;
                SetMoveTarget(chest->position.x + dx * inverseDistance * 1.05f,
                              chest->position.z + dz * inverseDistance * 1.05f);
            }
            return;
        }
    }

    if (std::fabs(ray.direction.y) < 0.0001f) return;
    const float floorDistance = -ray.position.y / ray.direction.y;
    if (floorDistance <= 0.0f) return;
    const Vector3 clickedFloor = {
        ray.position.x + ray.direction.x * floorDistance,
        0.0f,
        ray.position.z + ray.direction.z * floorDistance
    };
    if (clickedFloor.x < -4.4f || clickedFloor.x > 4.4f ||
        clickedFloor.z < -4.4f || clickedFloor.z > 4.4f) return;
    SetMoveTarget(clickedFloor.x, clickedFloor.z);
}
