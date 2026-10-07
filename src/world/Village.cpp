#include "world/Village.h"

#include "navigation/NavigationSystem.h"
#include "world/World.h"
#include <raymath.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace {

struct Building {
    float offsetX;
    float offsetZ;
    float halfWidth;
    float halfDepth;
    int frontSide;
};

Vector2 BuildingHalfExtents(const Building& building) {
    if (building.frontSide == 2 || building.frontSide == 3)
        return { building.halfDepth, building.halfWidth };
    return { building.halfWidth, building.halfDepth };
}

constexpr std::array<Building, 5> kBuildings = {{
    { 0.0f, -8.0f, 3.0f, 2.5f, 0 },
    { 8.0f, 0.0f, 2.5f, 2.5f, 2 },
    { 0.0f, 8.0f, 3.0f, 2.5f, 1 },
    { -8.0f, 0.0f, 2.5f, 2.5f, 3 },
    { -9.0f, 8.0f, 3.0f, 2.5f, 3 }
}};

constexpr std::array<Vector2, 8> kTreeOffsets = {{
    { -14.0f, -13.0f }, { 14.0f, -13.0f }, { -14.0f, 13.0f }, { 14.0f, 13.0f },
    { -18.0f, -4.0f }, { 18.0f, 5.0f }, { -5.0f, 17.0f }, { 6.0f, -17.0f }
}};

bool SampleDry(const World& world, float x, float z, float& minHeight, float& maxHeight) {
    if (world.IsWaterAt(x, z)) return false;
    const float height = world.GetHeightAt(x, z);
    minHeight = std::min(minHeight, height);
    maxHeight = std::max(maxHeight, height);
    return true;
}

bool IsVillageSite(const World& world, float centerX, float centerZ) {
    float minHeight = 100000.0f;
    float maxHeight = -100000.0f;
    auto sample = [&](float x, float z) {
        return SampleDry(world, x, z, minHeight, maxHeight);
    };

    for (int z = -2; z <= 2; ++z) {
        for (int x = -2; x <= 2; ++x) {
            if (!sample(centerX + x, centerZ + z)) return false;
        }
    }

    for (const Building& building : kBuildings) {
        const float buildingX = centerX + building.offsetX;
        const float buildingZ = centerZ + building.offsetZ;
        const Vector2 halfExtents = BuildingHalfExtents(building);
        for (int z = -static_cast<int>(halfExtents.y); z <= static_cast<int>(halfExtents.y); ++z) {
            for (int x = -static_cast<int>(halfExtents.x); x <= static_cast<int>(halfExtents.x); ++x) {
                if (!sample(buildingX + x, buildingZ + z)) return false;
            }
        }
    }

    for (int distance = 3; distance <= 6; ++distance) {
        if (!sample(centerX, centerZ - distance) || !sample(centerX + distance, centerZ) ||
            !sample(centerX, centerZ + distance) || !sample(centerX - distance, centerZ)) return false;
    }
    return maxHeight - minHeight <= 0.2f;
}

float BuildingGround(const World& world, float x, float z, float halfWidth, float halfDepth) {
    float highest = world.GetHeightAt(x, z);
    for (int dz = -static_cast<int>(halfDepth); dz <= static_cast<int>(halfDepth); ++dz) {
        for (int dx = -static_cast<int>(halfWidth); dx <= static_cast<int>(halfWidth); ++dx) {
            highest = std::max(highest, world.GetHeightAt(x + dx, z + dz));
        }
    }
    return highest;
}

void DrawPathTile(const World& world, float x, float z) {
    if (world.IsWaterAt(x, z)) return;
    const float y = world.GetHeightAt(x, z) + 0.035f;
    DrawCube({ x, y, z }, 0.98f, 0.07f, 0.98f, Color{ 128, 116, 96, 255 });
}

void DrawTree(float x, float z, float ground, float scale) {
    DrawCylinder({ x, ground, z }, 0.17f * scale, 0.27f * scale, 2.5f * scale, 8,
                 Color{ 91, 62, 39, 255 });
    DrawCylinder({ x, ground + 1.15f * scale, z }, 0.0f, 1.25f * scale, 2.1f * scale, 8,
                 Color{ 48, 94, 51, 255 });
    DrawCylinder({ x, ground + 1.85f * scale, z }, 0.0f, 0.92f * scale, 1.65f * scale, 8,
                 Color{ 59, 111, 58, 255 });
    DrawSphere({ x, ground + 3.0f * scale, z }, 0.48f * scale, Color{ 70, 123, 63, 255 });
}

void DrawResident(const VillageNpc& npc, Vector3 position) {
    if (!npc.model || npc.model->meshCount == 0) return;
    DrawModelEx(*npc.model, position, { 0.0f, 1.0f, 0.0f }, npc.facing,
                { 1.0f, 1.0f, 1.0f }, WHITE);
}

VillageNpc MakeNpc(const char* id, VillageNpcRole role, Vector3 position, Color clothing, int homeBuilding,
                   std::array<Vector2, 3> route, float waitTime) {
    VillageNpc npc;
    npc.id = id;
    npc.role = role;
    npc.position = position;
    npc.clothing = clothing;
    npc.homeBuilding = homeBuilding;
    npc.route = route;
    npc.routeCount = static_cast<int>(route.size());
    npc.target = position;
    npc.waitTime = waitTime;
    return npc;
}

Vector3 DoorPosition(Vector3 center, const Building& building) {
    float x = center.x + building.offsetX;
    float z = center.z + building.offsetZ;
    if (building.frontSide == 0) z += building.halfDepth + 0.5f;
    else if (building.frontSide == 1) z -= building.halfDepth + 0.5f;
    else if (building.frontSide == 2) x -= building.halfDepth + 0.5f;
    else x += building.halfDepth + 0.5f;
    return { x, 0.0f, z };
}

void DrawCandle(Vector3 position, float radius, float height, int slices, Color flameColor) {
    DrawCylinder(position, radius, radius, height, slices, Color{ 218, 203, 164, 255 });
    DrawSphere({ position.x, position.y + height + radius, position.z }, radius * 1.15f, flameColor);
}

}

Village::Village(Vector3 center) : m_center(center) {
    m_npcs = {
        MakeNpc("npc.blacksmith", VillageNpcRole::Blacksmith, { center.x - 2.0f, center.y, center.z - 3.0f },
            Color{ 88, 79, 72, 255 }, 0, {{{-2, -3}, {-3, -2}, {-1, -2}}}, 1.2f),
        MakeNpc("npc.mayor", VillageNpcRole::Mayor, { center.x + 5.0f, center.y, center.z + 2.0f },
                Color{ 47, 65, 98, 255 }, 1, {{{5, 2}, {4, 3}, {3, 3}}}, 2.1f),
        MakeNpc("npc.cleric", VillageNpcRole::Cleric, { center.x + 2.0f, center.y, center.z + 5.0f },
                Color{ 211, 198, 166, 255 }, 2, {{{2, 5}, {1, 4}, {2, 6}}}, 0.8f),
        MakeNpc("npc.farmer", VillageNpcRole::Farmer, { center.x - 6.0f, center.y, center.z + 8.0f },
            Color{ 82, 105, 54, 255 }, 4, {{{-6, 8}, {-7, 8}, {-6, 9}}}, 3.0f),
        MakeNpc("npc.innkeeper", VillageNpcRole::Innkeeper, { center.x - 5.0f, center.y, center.z - 2.0f },
                Color{ 113, 66, 45, 255 }, 3, {{{-5, -2}, {-4, -3}, {-5, -4}}}, 1.7f)
    };
    m_npcs[1].insideHome = true;
    m_npcs[2].insideHome = true;
}

void Village::SetBuildingModel(int index, Model* model) {
    if (index >= 0 && index < static_cast<int>(m_buildingModels.size()))
        m_buildingModels[static_cast<std::size_t>(index)] = model;
}

void Village::SetNpcAssets(int index, Model* model, ModelAnimation* animations, int animationCount) {
    if (index < 0 || index >= static_cast<int>(m_npcs.size())) return;
    VillageNpc& npc = m_npcs[static_cast<std::size_t>(index)];
    npc.model = model;
    npc.animations = animations;
    npc.animationCount = animationCount;
}

void Village::HighlightBuilding(int index) {
    if (index < 0 || index >= static_cast<int>(kBuildings.size())) return;
    m_highlightedBuilding = index;
    m_highlightTimer = 0.55f;
}

void Village::Update(const World& world, const NavigationSystem& navigation, float deltaTime, float timeOfDay) {
    m_highlightTimer = std::max(0.0f, m_highlightTimer - deltaTime);
    if (m_highlightTimer == 0.0f) m_highlightedBuilding = -1;
    const bool night = timeOfDay < 0.18f || timeOfDay > 0.82f;

    for (VillageNpc& npc : m_npcs) {
        const Vector3 homeDoor = DoorPosition(m_center, kBuildings[static_cast<std::size_t>(npc.homeBuilding)]);
        npc.position.y = world.GetHeightAt(npc.position.x, npc.position.z);
        const bool daytimeIndoor = npc.role == VillageNpcRole::Mayor || npc.role == VillageNpcRole::Cleric ||
            (npc.role == VillageNpcRole::Innkeeper && timeOfDay >= 0.48f && timeOfDay < 0.66f);
        const bool wantsHome = night || daytimeIndoor;
        const auto setDestination = [&](Vector3 destination) {
            const std::vector<TileCoord> path = navigation.FindPath(
                static_cast<int>(std::floor(npc.position.x)), static_cast<int>(std::floor(npc.position.z)),
                static_cast<int>(std::floor(destination.x)), static_cast<int>(std::floor(destination.z)));
            if (path.empty()) return false;
            npc.target = destination;
            npc.waypoints.clear();
            npc.waypoints.reserve(path.size());
            for (const TileCoord& tile : path) {
                const float x = static_cast<float>(tile.x) + 0.5f;
                const float z = static_cast<float>(tile.z) + 0.5f;
                npc.waypoints.push_back({ x, world.GetHeightAt(x, z), z });
            }
            npc.waypointIndex = path.size() > 1 ? 1 : 0;
            return true;
        };

        if (wantsHome && !npc.insideHome && !npc.returningHome) {
            npc.returningHome = setDestination(homeDoor);
            if (!npc.returningHome) npc.waitTime = 0.8f;
        } else if (!wantsHome && npc.insideHome) {
            npc.insideHome = false;
            npc.position = homeDoor;
            npc.target = homeDoor;
            npc.waypoints.clear();
            npc.waypointIndex = 0;
            npc.returningHome = false;
            npc.waitTime = 0.4f;
        }

        if (npc.insideHome) {
            npc.walking = false;
        } else {
            if (npc.waypointIndex < npc.waypoints.size()) {
                const Vector3 waypoint = npc.waypoints[npc.waypointIndex];
                const float dx = waypoint.x - npc.position.x;
                const float dz = waypoint.z - npc.position.z;
                const float distance = std::sqrt(dx * dx + dz * dz);
                const float step = 1.25f * deltaTime;
                if (distance <= std::max(step, 0.04f)) {
                    npc.position = waypoint;
                    ++npc.waypointIndex;
                    npc.walking = false;
                    if (npc.waypointIndex >= npc.waypoints.size()) {
                        npc.waypoints.clear();
                        npc.waypointIndex = 0;
                        if (npc.returningHome) {
                            npc.insideHome = true;
                            npc.returningHome = false;
                        } else {
                            npc.waitTime = 2.4f + static_cast<float>(npc.routeIndex % 3) * 0.8f;
                        }
                    }
                } else {
                    npc.position.x += dx / distance * step;
                    npc.position.z += dz / distance * step;
                    npc.position.y = world.GetHeightAt(npc.position.x, npc.position.z);
                    npc.facing = std::atan2(dx, dz) * RAD2DEG;
                    npc.walking = true;
                }
            } else {
                npc.walking = false;
                npc.waitTime = std::max(0.0f, npc.waitTime - deltaTime);
                if (!wantsHome && npc.waitTime <= 0.0f) {
                    npc.routeIndex = (npc.routeIndex + 1) % npc.routeCount;
                    const Vector2 offset = npc.route[static_cast<std::size_t>(npc.routeIndex)];
                    const Vector3 destination{ m_center.x + offset.x, 0.0f, m_center.z + offset.y };
                    if (!setDestination(destination)) npc.waitTime = 1.0f;
                } else if (wantsHome && !npc.returningHome) {
                    npc.returningHome = setDestination(homeDoor);
                }
            }
        }

        if (npc.model && npc.animations && npc.animationCount > 1) {
            const bool worksAtTool = !night && !npc.walking &&
                (npc.role == VillageNpcRole::Blacksmith || npc.role == VillageNpcRole::Farmer);
            const int clip = npc.walking ? 1 : (worksAtTool && npc.animationCount > 7 ? 7 : 0);
            if (npc.appliedAnimation != clip) npc.animationTime = 0.0f;
            npc.animationTime += deltaTime;
            const float duration = clip == 0 ? 2.4f : (clip == 1 ? 1.0f : 1.25f);
            const ModelAnimation& animation = npc.animations[clip];
            int frame = static_cast<int>(std::fmod(npc.animationTime, duration) / duration * animation.frameCount);
            frame = std::clamp(frame, 0, animation.frameCount - 1);
            if (npc.appliedAnimation != clip || npc.appliedFrame != frame) {
                UpdateModelAnimation(*npc.model, animation, frame);
                npc.appliedAnimation = clip;
                npc.appliedFrame = frame;
            }
        }

        if (npc.walking) npc.waitTime = 0.0f;
    }
}

Vector3 Village::FindSpawnPosition(const World& world) {
    for (int radius = 0; radius <= 48; ++radius) {
        for (int x = -radius; x <= radius; ++x) {
            for (int side : { -1, 1 }) {
                const int z = side * radius;
                const float centerX = x + 0.5f;
                const float centerZ = z + 0.5f;
                if (IsVillageSite(world, centerX, centerZ)) {
                    return { centerX, world.GetHeightAt(centerX, centerZ), centerZ };
                }
            }
        }
        for (int z = -radius + 1; z < radius; ++z) {
            for (int side : { -1, 1 }) {
                const int x = side * radius;
                const float centerX = x + 0.5f;
                const float centerZ = z + 0.5f;
                if (IsVillageSite(world, centerX, centerZ)) {
                    return { centerX, world.GetHeightAt(centerX, centerZ), centerZ };
                }
            }
        }
    }

    for (int radius = 0; radius <= 64; ++radius) {
        for (int x = -radius; x <= radius; ++x) {
            for (int z : { -radius, radius }) {
                const float centerX = x + 0.5f;
                const float centerZ = z + 0.5f;
                if (!world.IsWaterAt(centerX, centerZ)) return { centerX, world.GetHeightAt(centerX, centerZ), centerZ };
            }
        }
    }
    return { 0.5f, world.GetHeightAt(0.5f, 0.5f), 0.5f };
}

std::vector<Rectangle> Village::CollisionBounds() const {
    std::vector<Rectangle> bounds;
    bounds.reserve(kBuildings.size() * 4 + kTreeOffsets.size() + 1);
    const auto addObstacle = [&](float centerX, float centerZ, float halfX, float halfZ) {
        bounds.push_back({ centerX - halfX, centerZ - halfZ, halfX * 2.0f, halfZ * 2.0f });
    };
    for (const Building& building : kBuildings) {
        const Vector2 halfExtents = BuildingHalfExtents(building);
        const float centerX = m_center.x + building.offsetX;
        const float centerZ = m_center.z + building.offsetZ;
        const float wall = 0.34f;
        const float doorHalfWidth = 0.58f;
        const bool doorAlongZ = building.frontSide == 0 || building.frontSide == 1;
        const float frontSign = building.frontSide == 0 || building.frontSide == 3 ? 1.0f : -1.0f;
        if (doorAlongZ) {
            addObstacle(centerX - halfExtents.x, centerZ, wall, halfExtents.y + wall);
            addObstacle(centerX + halfExtents.x, centerZ, wall, halfExtents.y + wall);
            addObstacle(centerX, centerZ - frontSign * halfExtents.y, halfExtents.x + wall, wall);
            const float frontZ = centerZ + frontSign * halfExtents.y;
            const float segmentHalf = std::max(0.0f, (halfExtents.x - doorHalfWidth) * 0.5f);
            const float segmentCenter = (halfExtents.x + doorHalfWidth) * 0.5f;
            addObstacle(centerX - segmentCenter, frontZ, segmentHalf + wall, wall);
            addObstacle(centerX + segmentCenter, frontZ, segmentHalf + wall, wall);
        } else {
            addObstacle(centerX, centerZ - halfExtents.y, halfExtents.x + wall, wall);
            addObstacle(centerX, centerZ + halfExtents.y, halfExtents.x + wall, wall);
            addObstacle(centerX - frontSign * halfExtents.x, centerZ, wall, halfExtents.y + wall);
            const float frontX = centerX + frontSign * halfExtents.x;
            const float segmentHalf = std::max(0.0f, (halfExtents.y - doorHalfWidth) * 0.5f);
            const float segmentCenter = (halfExtents.y + doorHalfWidth) * 0.5f;
            addObstacle(frontX, centerZ - segmentCenter, wall, segmentHalf + wall);
            addObstacle(frontX, centerZ + segmentCenter, wall, segmentHalf + wall);
        }
    }
    addObstacle(m_center.x + 1.9f, m_center.z - 1.9f, 1.12f, 1.12f);
    for (const Vector2 offset : kTreeOffsets)
        addObstacle(m_center.x + offset.x, m_center.z + offset.y, 0.42f, 0.42f);
    return bounds;
}

const VillageNpc* Village::NpcAtTile(int tileX, int tileZ) const {
    for (const VillageNpc& npc : m_npcs) {
        if (static_cast<int>(std::floor(npc.position.x)) == tileX &&
            static_cast<int>(std::floor(npc.position.z)) == tileZ) return &npc;
    }
    return nullptr;
}

const VillageNpc* Village::NpcAtRay(Ray ray) const {
    const VillageNpc* closestNpc = nullptr;
    float closestDistance = 100000.0f;
    for (const VillageNpc& npc : m_npcs) {
        if (npc.insideHome) continue;
        const BoundingBox bounds = {
            { npc.position.x - 0.38f, npc.position.y, npc.position.z - 0.38f },
            { npc.position.x + 0.38f, npc.position.y + 1.9f, npc.position.z + 0.38f }
        };
        const RayCollision hit = GetRayCollisionBox(ray, bounds);
        if (hit.hit && hit.distance < closestDistance) {
            closestNpc = &npc;
            closestDistance = hit.distance;
        }
    }
    return closestNpc;
}

bool Village::NpcAtInteriorRay(Ray ray, int buildingIndex, VillageNpc& outNpc) const {
    int interiorNpcIndex = 0;
    float closestDistance = 100000.0f;
    bool found = false;
    for (const VillageNpc& npc : m_npcs) {
        if (!npc.insideHome || npc.homeBuilding != buildingIndex) continue;
        const Vector3 position{ -2.6f + (interiorNpcIndex % 2) * 1.1f, 0.0f,
                                -0.5f + (interiorNpcIndex / 2) * 1.2f };
        ++interiorNpcIndex;
        const BoundingBox bounds = {
            { position.x - 0.38f, position.y, position.z - 0.38f },
            { position.x + 0.38f, position.y + 1.9f, position.z + 0.38f }
        };
        const RayCollision hit = GetRayCollisionBox(ray, bounds);
        if (hit.hit && hit.distance < closestDistance) {
            closestDistance = hit.distance;
            outNpc = npc;
            outNpc.position = position;
            found = true;
        }
    }
    return found;
}

int Village::BuildingDoorAtTile(int tileX, int tileZ) const {
    for (int index = 0; index < static_cast<int>(kBuildings.size()); ++index) {
        const Building& building = kBuildings[static_cast<std::size_t>(index)];
        const Vector3 door = DoorPosition(m_center, building);
        const float doorX = door.x;
        const float doorZ = door.z;
        if (static_cast<int>(std::floor(doorX)) == tileX &&
            static_cast<int>(std::floor(doorZ)) == tileZ) return index;
    }
    return -1;
}

int Village::BuildingDoorAtRay(const World& world, Ray ray) const {
    int closestBuilding = -1;
    float closestDistance = 100000.0f;
    for (int index = 0; index < static_cast<int>(kBuildings.size()); ++index) {
        const Building& building = kBuildings[static_cast<std::size_t>(index)];
        const float x = m_center.x + building.offsetX;
        const float z = m_center.z + building.offsetZ;
        const Vector2 halfExtents = BuildingHalfExtents(building);
        const float ground = BuildingGround(world, x, z, halfExtents.x, halfExtents.y);
        BoundingBox doorBounds{};
        doorBounds.min.y = ground + 0.42f;
        doorBounds.max.y = ground + 2.15f;
        if (building.frontSide == 0) {
            doorBounds.min = { x - 0.58f, doorBounds.min.y, z + building.halfDepth + 0.02f };
            doorBounds.max = { x + 0.58f, doorBounds.max.y, z + building.halfDepth + 0.22f };
        } else if (building.frontSide == 1) {
            doorBounds.min = { x - 0.58f, doorBounds.min.y, z - building.halfDepth - 0.22f };
            doorBounds.max = { x + 0.58f, doorBounds.max.y, z - building.halfDepth - 0.02f };
        } else if (building.frontSide == 2) {
            doorBounds.min = { x - building.halfDepth - 0.22f, doorBounds.min.y, z - 0.58f };
            doorBounds.max = { x - building.halfDepth - 0.02f, doorBounds.max.y, z + 0.58f };
        } else {
            doorBounds.min = { x + building.halfDepth + 0.02f, doorBounds.min.y, z - 0.58f };
            doorBounds.max = { x + building.halfDepth + 0.22f, doorBounds.max.y, z + 0.58f };
        }

        const RayCollision collision = GetRayCollisionBox(ray, doorBounds);
        if (collision.hit && collision.distance < closestDistance) {
            closestBuilding = index;
            closestDistance = collision.distance;
        }
    }
    return closestBuilding;
}

std::vector<Vector3> Village::NpcPositions() const {
    std::vector<Vector3> positions;
    positions.reserve(m_npcs.size());
    for (const VillageNpc& npc : m_npcs) {
        if (!npc.insideHome) positions.push_back(npc.position);
    }
    return positions;
}

std::string Village::BuildingNameKey(int buildingIndex) {
    switch (buildingIndex) {
        case 0: return "village.building.forge";
        case 1: return "village.building.hall";
        case 2: return "village.building.chapel";
        case 3: return "village.building.inn";
        case 4: return "village.building.farmhouse";
        default: return "village.building.home";
    }
}

void Village::Render(const World& world) const {
    for (int z = -2; z <= 2; ++z) {
        for (int x = -2; x <= 2; ++x) DrawPathTile(world, m_center.x + x, m_center.z + z);
    }

    for (int distance = 3; distance <= 6; ++distance) {
        DrawPathTile(world, m_center.x, m_center.z - distance);
        DrawPathTile(world, m_center.x + distance, m_center.z);
        DrawPathTile(world, m_center.x, m_center.z + distance);
        DrawPathTile(world, m_center.x - distance, m_center.z);
    }

    for (int index = 0; index < static_cast<int>(kBuildings.size()); ++index) {
        const Building& building = kBuildings[static_cast<std::size_t>(index)];
        const float x = m_center.x + building.offsetX;
        const float z = m_center.z + building.offsetZ;
        const Vector2 halfExtents = BuildingHalfExtents(building);
        const float ground = BuildingGround(world, x, z, halfExtents.x, halfExtents.y) + 0.04f;
        Model* model = m_buildingModels[static_cast<std::size_t>(index)];
        if (model && model->meshCount > 0) {
            const Color tint = index == m_highlightedBuilding && m_highlightTimer > 0.0f
                ? Color{ 255, 218, 102, 255 } : WHITE;
            const float rotation = building.frontSide == 1 ? 180.0f
                : (building.frontSide == 2 ? -90.0f : (building.frontSide == 3 ? 90.0f : 0.0f));
            DrawModelEx(*model, { x, ground, z }, { 0.0f, 1.0f, 0.0f }, rotation,
                        { 1.0f, 1.0f, 1.0f }, tint);
        }
        if (index == m_highlightedBuilding && m_highlightTimer > 0.0f) {
            DrawCubeWires({ x, ground + 2.7f, z }, halfExtents.x * 2.0f + 1.0f,
                          5.7f, halfExtents.y * 2.0f + 1.0f, GOLD);
        }
    }

    const float wellX = m_center.x + 1.9f;
    const float wellZ = m_center.z - 1.9f;
    const float wellGround = world.GetHeightAt(wellX, wellZ) + 0.06f;
    DrawCylinder({ wellX, wellGround, wellZ }, 0.78f, 0.82f, 0.85f, 12, Color{ 114, 105, 88, 255 });
    DrawCylinder({ wellX, wellGround + 0.83f, wellZ }, 0.57f, 0.58f, 0.08f, 12, Color{ 29, 55, 66, 255 });
    DrawCube({ wellX - 0.72f, wellGround + 1.45f, wellZ }, 0.16f, 1.25f, 0.16f, Color{ 73, 49, 33, 255 });
    DrawCube({ wellX + 0.72f, wellGround + 1.45f, wellZ }, 0.16f, 1.25f, 0.16f, Color{ 73, 49, 33, 255 });
    DrawCube({ wellX, wellGround + 2.12f, wellZ }, 1.9f, 0.16f, 1.25f, Color{ 84, 43, 36, 255 });

    for (const Vector2 offset : kTreeOffsets) {
        const float x = m_center.x + offset.x;
        const float z = m_center.z + offset.y;
        if (!world.IsWaterAt(x, z)) DrawTree(x, z, world.GetHeightAt(x, z), 0.9f);
    }

    for (const VillageNpc& npc : m_npcs) {
        if (!npc.insideHome) {
            Vector3 position = npc.position;
            position.y = world.GetHeightAt(position.x, position.z) + 0.04f;
            DrawResident(npc, position);
        }
    }
}

void Village::RenderInterior(int buildingIndex) const {
    const Color timber{ 83, 58, 39, 255 };
    const Color plaster{ 142, 115, 82, 255 };
    const Color floor{ 105, 80, 58, 255 };
    DrawCube({ 0.0f, -0.18f, 0.0f }, 10.0f, 0.36f, 9.0f, floor);
    DrawCube({ 0.0f, 1.5f, -4.35f }, 10.0f, 3.0f, 0.3f, plaster);
    DrawCube({ -4.85f, 1.5f, 0.0f }, 0.3f, 3.0f, 9.0f, plaster);
    DrawCube({ 4.85f, 1.5f, 0.0f }, 0.3f, 3.0f, 9.0f, plaster);
    DrawCube({ -2.65f, 1.5f, 4.35f }, 4.7f, 3.0f, 0.3f, plaster);
    DrawCube({ 2.65f, 1.5f, 4.35f }, 4.7f, 3.0f, 0.3f, plaster);
    DrawCube({ 0.0f, 2.95f, 4.35f }, 1.0f, 0.2f, 0.3f, timber);

    if (buildingIndex == 0) {
        DrawCube({ -3.0f, 0.55f, -2.0f }, 2.0f, 1.1f, 1.3f, Color{ 65, 57, 50, 255 });
        DrawCube({ -3.0f, 1.2f, -2.0f }, 1.0f, 0.35f, 0.6f, ORANGE);
        DrawCube({ 1.0f, 0.52f, 0.0f }, 2.4f, 1.0f, 1.0f, timber);
        DrawCube({ 1.0f, 1.08f, 0.0f }, 0.25f, 0.12f, 0.25f, LIGHTGRAY);
    } else if (buildingIndex == 1) {
        DrawCube({ 0.0f, 0.62f, -1.6f }, 3.8f, 1.2f, 1.25f, timber);
        DrawCube({ -3.4f, 1.45f, -2.7f }, 0.35f, 2.5f, 1.3f, DARKBROWN);
        DrawCube({ -3.4f, 1.9f, -1.7f }, 0.35f, 0.08f, 1.3f, GOLD);
        DrawCube({ 3.4f, 1.45f, -2.7f }, 0.35f, 2.5f, 1.3f, DARKBROWN);
    } else if (buildingIndex == 2) {
        DrawCube({ 0.0f, 0.42f, -2.7f }, 2.6f, 0.8f, 1.0f, Color{ 174, 156, 119, 255 });
        DrawCube({ 0.0f, 1.2f, -2.7f }, 0.12f, 1.0f, 0.12f, GOLD);
        DrawCube({ 0.0f, 1.45f, -2.7f }, 0.5f, 0.12f, 0.12f, GOLD);
        DrawCandle({ -1.0f, 0.85f, -2.7f }, 0.12f, 0.35f, 8, Color{ 240, 174, 65, 255 });
        DrawCandle({ 1.0f, 0.85f, -2.7f }, 0.12f, 0.35f, 8, Color{ 240, 174, 65, 255 });
    } else {
        DrawCube({ 0.0f, 0.65f, -3.0f }, 6.6f, 1.3f, 0.8f, Color{ 105, 67, 42, 255 });
        DrawCube({ -2.7f, 0.35f, 0.0f }, 1.5f, 0.7f, 1.3f, timber);
        DrawCube({ 2.7f, 0.35f, 0.0f }, 1.5f, 0.7f, 1.3f, timber);
        DrawCube({ 0.0f, 0.34f, 1.7f }, 2.0f, 0.68f, 1.1f, timber);
    }

    int interiorNpcIndex = 0;
    for (const VillageNpc& npc : m_npcs) {
        if (!npc.insideHome || npc.homeBuilding != buildingIndex) continue;
        const Vector3 position{ -2.6f + (interiorNpcIndex % 2) * 1.1f, 0.0f,
                                -0.5f + (interiorNpcIndex / 2) * 1.2f };
        DrawResident(npc, position);
        ++interiorNpcIndex;
    }
}
