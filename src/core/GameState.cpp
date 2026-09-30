#include "core/GameState.h"
#include "core/Game.h"
#include "rendering/Renderer.h"
#include "rendering/Camera3DController.h"
#include "world/World.h"
#include "core/Timer.h"
#include <raylib.h>
#include <raymath.h>
#include <iostream>

GameState::GameState(Game* game)
    : m_game(game)
    , m_type(GameStateType::Playing)
{
}

// Playing State Implementation
PlayingState::PlayingState(Game* game)
    : GameState(game)
    , m_showDebugInfo(true)
{
    m_type = GameStateType::Playing;
}

void PlayingState::Enter() {}

void PlayingState::Exit() {}

void PlayingState::Update(float deltaTime) {
    if (auto* world = m_game->GetWorld()) {
        world->Update(deltaTime);
    }
}

void PlayingState::Render() {
    auto* renderer = m_game->GetRenderer();
    auto* camera = m_game->GetCamera();
    auto* world = m_game->GetWorld();

    if (!renderer || !camera) return;

    // Begin 3D rendering
    renderer->BeginMode3D(camera->GetCamera());

    renderer->RenderWorld();
    renderer->EndMode3D();

    // Render UI
    renderer->RenderUI();
}

void PlayingState::HandleInput() {
    if (IsKeyPressed(KEY_F3)) {
        m_showDebugInfo = !m_showDebugInfo;
    }

    // STEP 4: mouse click -> selected tile (input layer, not Chunk)
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (auto* camera = m_game->GetCamera()) {
            Ray ray = GetMouseRay(GetMousePosition(), camera->GetCamera());
            // Intersection with ground plane y ≈ 0 (terrain base / block bottom)
            float dist = 0.0f;
            Vector3 pt = {0};
            // Simple plane intersection at y=0
            if (ray.direction.y != 0) {
                float t = -ray.position.y / ray.direction.y;
                if (t > 0) {
                    pt = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
                    int tileX = static_cast<int>(std::floor(pt.x / 1.0f));
                    int tileZ = static_cast<int>(std::floor(pt.z / 1.0f));
                    std::cout << "SELECTED TILE: world = (" << pt.x << "," << pt.z << ") tile = (" << tileX << "," << tileZ << ")" << std::endl;
                    // Tile-based movement: Player.MoveToTile(tileX, tileZ) called here when Player integrated
                }
            }
        }
    }
}