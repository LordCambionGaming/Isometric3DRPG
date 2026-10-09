#include "world/Village.h"

#include "navigation/NavigationSystem.h"
#include "world/World.h"
#include <nlohmann/json.hpp>
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <random>
#include <unordered_set>


namespace {

const nlohmann::json& VillageLayout() {
    static const nlohmann::json layout = [] {
        std::ifstream layoutFile("assets/models/village/layout.json");
        if (!layoutFile) {
            TraceLog(LOG_ERROR, "Village layout not found: assets/models/village/layout.json");
            return nlohmann::json::object();
        }
        try {
            nlohmann::json data;
            layoutFile >> data;
            return data;
        } catch (const nlohmann::json::exception& error) {
            TraceLog(LOG_ERROR, "Invalid village layout: %s", error.what());
            return nlohmann::json::object();
        }
    }();
    return layout;
}

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

    for (int z = -4; z <= 4; ++z) {
        for (int x = -4; x <= 4; ++x) {
            if (!sample(centerX + x, centerZ + z)) return false;
        }
    }

    const auto& instances = VillageLayout().value("instances", nlohmann::json::array());
    for (const auto& instance : instances) {
        if (!instance.contains("kind") || !instance.contains("pos") || !instance["pos"].is_array()) continue;
        const float houseX = centerX + instance["pos"][0].get<float>();
        const float houseZ = centerZ + instance["pos"][2].get<float>();
        const float halfWidth = instance.value("half_width", 2.5f);
        const float halfDepth = instance.value("half_depth", 2.0f);
        const float yaw = instance.value("yaw_deg", 0.0f) * DEG2RAD;
        for (int localZ = -static_cast<int>(std::ceil(halfDepth));
             localZ <= static_cast<int>(std::ceil(halfDepth)); ++localZ) {
            for (int localX = -static_cast<int>(std::ceil(halfWidth));
                 localX <= static_cast<int>(std::ceil(halfWidth)); ++localX) {
                const float x = houseX + localX * std::cos(yaw) + localZ * std::sin(yaw);
                const float z = houseZ - localX * std::sin(yaw) + localZ * std::cos(yaw);
                if (!sample(x, z)) return false;
            }
        }
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

void DrawResident(const VillageNpc& npc, Vector3 position) {
    if (!npc.model || npc.model->meshCount == 0) return;
    DrawModelEx(*npc.model, position, { 0.0f, 1.0f, 0.0f }, npc.facing,
                { 1.0f, 1.0f, 1.0f }, WHITE);
}

VillageNpc MakeNpc(const char* id, VillageNpcRole role, Vector3 position, Color clothing,
                   int homeBuilding, float wanderRadius, float waitTime) {
    VillageNpc npc;
    npc.id = id;
    npc.role = role;
    npc.position = position;
    npc.spawnPos = position;
    npc.clothing = clothing;
    npc.homeBuilding = homeBuilding;
    npc.wanderRadius = wanderRadius;
    npc.target = position;
    npc.waitTime = waitTime;
    return npc;
}

Vector2 RotateLocal(float x, float z, float yawDegrees) {
    const float yaw = yawDegrees * DEG2RAD;
    return { x * std::cos(yaw) + z * std::sin(yaw),
             -x * std::sin(yaw) + z * std::cos(yaw) };
}

template <typename BuildingType>
Vector3 DoorPosition(Vector3 center, const BuildingType& building) {
    const Vector2 offset = RotateLocal(building.doorStep.x, building.doorStep.z, building.yawDegrees);
    return { center.x + building.position.x + offset.x, center.y + building.doorStep.y,
             center.z + building.position.z + offset.y };
}

bool IsChunkVisible(const World& world, Vector3 position, Vector3 playerPosition) {
    int chunkX = 0;
    int chunkZ = 0;
    int playerChunkX = 0;
    int playerChunkZ = 0;
    world.WorldToChunk(position.x, position.z, chunkX, chunkZ);
    world.WorldToChunk(playerPosition.x, playerPosition.z, playerChunkX, playerChunkZ);
    return world.IsChunkLoaded(chunkX, chunkZ) &&
        std::abs(chunkX - playerChunkX) <= world.GetRenderRadius() &&
        std::abs(chunkZ - playerChunkZ) <= world.GetRenderRadius();
}

std::string BuildingKindForRole(VillageNpcRole role) {
    switch (role) {
        case VillageNpcRole::Blacksmith: return "forge";
        case VillageNpcRole::Mayor: return "hall";
        case VillageNpcRole::Cleric: return "chapel";
        case VillageNpcRole::Farmer: return "farmhouse";
        case VillageNpcRole::Innkeeper: return "inn";
    }
    return {};
}

void DrawCandle(Vector3 position, float radius, float height, int slices, Color flameColor) {
    DrawCylinder(position, radius, radius, height, slices, Color{ 218, 203, 164, 255 });
    DrawSphere({ position.x, position.y + height + radius, position.z }, radius * 1.15f, flameColor);
}

// Thread-local RNG for NPC wandering — avoids global state issues.
std::mt19937& NpcRng() {
    static thread_local std::mt19937 rng(std::random_device{}());
    return rng;
}

} // namespace

Village::Village(Vector3 center) : m_center(center) {
    const nlohmann::json& layout = VillageLayout();
    std::unordered_set<std::string> uniqueAssets;
    if (layout.contains("instances") && layout["instances"].is_array()) {
        for (const auto& data : layout["instances"]) {
            if (!data.contains("asset") || !data["asset"].is_string() || !data.contains("pos") ||
                !data["pos"].is_array() || data["pos"].size() < 3) continue;
            Instance instance;
            instance.asset = data["asset"].get<std::string>();
            instance.position = { data["pos"][0].get<float>(), data["pos"][1].get<float>(),
                                  data["pos"][2].get<float>() };
            instance.yawDegrees = data.value("yaw_deg", 0.0f);
            instance.scale = data.value("scale", 1.0f);
            instance.collisionRadius = data.value("collision_radius", 0.0f);
            if (uniqueAssets.insert(instance.asset).second) m_modelAssets.push_back(instance.asset);

            if (data.contains("kind") && data["kind"].is_string()) {
                Building building;
                building.kind = data["kind"].get<std::string>();
                building.position = instance.position;
                building.yawDegrees = instance.yawDegrees;
                building.halfWidth = data.value("half_width", 2.5f);
                building.halfDepth = data.value("half_depth", 2.0f);
                if (data.contains("door_step_pos") && data["door_step_pos"].is_array() &&
                    data["door_step_pos"].size() >= 3) {
                    building.doorStep = { data["door_step_pos"][0].get<float>(),
                                          data["door_step_pos"][1].get<float>(),
                                          data["door_step_pos"][2].get<float>() };
                }
                if (data.contains("road_path") && data["road_path"].is_array()) {
                    for (const auto& point : data["road_path"]) {
                        if (point.is_array() && point.size() >= 2)
                            building.roadPath.emplace_back(point[0].get<float>(), point[1].get<float>());
                    }
                }
                building.instanceIndex = static_cast<int>(m_instances.size());
                instance.buildingIndex = static_cast<int>(m_buildings.size());
                m_buildings.push_back(std::move(building));
            }
            m_instances.push_back(std::move(instance));
        }
    }

    std::unordered_set<std::int64_t> remainingRoadTiles;
    for (const Building& building : m_buildings) {
        for (std::size_t segment = 1; segment < building.roadPath.size(); ++segment) {
            const Vector2 from = building.roadPath[segment - 1];
            const Vector2 to = building.roadPath[segment];
            const float distance = std::fabs(to.x - from.x) + std::fabs(to.y - from.y);
            const int steps = std::max(1, static_cast<int>(std::ceil(distance)));
            for (int step = 0; step <= steps; ++step) {
                const float amount = static_cast<float>(step) / static_cast<float>(steps);
                const int tileX = static_cast<int>(std::floor(m_center.x + from.x + (to.x - from.x) * amount));
                const int tileZ = static_cast<int>(std::floor(m_center.z + from.y + (to.y - from.y) * amount));
                const std::int64_t key = (static_cast<std::int64_t>(static_cast<std::uint32_t>(tileX)) << 32) |
                    static_cast<std::uint32_t>(tileZ);
                remainingRoadTiles.insert(key);
            }
        }
    }
    std::vector<std::pair<int, int>> orderedRoadTiles;
    orderedRoadTiles.reserve(remainingRoadTiles.size());
    for (const std::int64_t key : remainingRoadTiles) {
        orderedRoadTiles.emplace_back(static_cast<std::int32_t>(static_cast<std::uint64_t>(key) >> 32),
                                      static_cast<std::int32_t>(static_cast<std::uint32_t>(key)));
    }
    std::sort(orderedRoadTiles.begin(), orderedRoadTiles.end(), [](const auto& left, const auto& right) {
        return left.second == right.second ? left.first < right.first : left.second < right.second;
    });
    const auto roadKey = [](int tileX, int tileZ) {
        return (static_cast<std::int64_t>(static_cast<std::uint32_t>(tileX)) << 32) |
            static_cast<std::uint32_t>(tileZ);
    };
    for (const auto& tile : orderedRoadTiles) {
        if (remainingRoadTiles.erase(roadKey(tile.first, tile.second)) == 0) continue;
        int tileCount = 1;
        while (remainingRoadTiles.erase(roadKey(tile.first + tileCount, tile.second)) > 0) ++tileCount;
        m_roadRuns.push_back({ { tile.first + 0.5f, tile.second + 0.5f }, tileCount, true });
    }
    while (!remainingRoadTiles.empty()) {
        const std::uint64_t key = static_cast<std::uint64_t>(*remainingRoadTiles.begin());
        const int tileX = static_cast<std::int32_t>(key >> 32);
        const int tileZ = static_cast<std::int32_t>(static_cast<std::uint32_t>(key));
        remainingRoadTiles.erase(static_cast<std::int64_t>(key));
        int tileCount = 1;
        while (remainingRoadTiles.erase(roadKey(tileX, tileZ + tileCount)) > 0) ++tileCount;
        m_roadRuns.push_back({ { tileX + 0.5f, tileZ + 0.5f }, tileCount, false });
    }

    const auto homeFor = [&](VillageNpcRole role, int fallback) {
        const std::string kind = BuildingKindForRole(role);
        for (int index = 0; index < static_cast<int>(m_buildings.size()); ++index) {
            if (m_buildings[static_cast<std::size_t>(index)].kind == kind) return index;
        }
        return m_buildings.empty() ? -1 : fallback % static_cast<int>(m_buildings.size());
    };
    const auto homePosition = [&](int index, float lateralOffset = 0.0f) {
        if (index < 0 || index >= static_cast<int>(m_buildings.size())) return center;
        const Building& building = m_buildings[static_cast<std::size_t>(index)];
        const Vector2 offset = RotateLocal(lateralOffset, building.doorStep.z, building.yawDegrees);
        return Vector3{ center.x + building.position.x + offset.x,
                        center.y + building.doorStep.y,
                        center.z + building.position.z + offset.y };
    };
    const int blacksmithHome = homeFor(VillageNpcRole::Blacksmith, 0);
    const int mayorHome = homeFor(VillageNpcRole::Mayor, 1);
    const int clericHome = homeFor(VillageNpcRole::Cleric, 2);
    const int farmerHome = homeFor(VillageNpcRole::Farmer, 3);
    const int innkeeperHome = homeFor(VillageNpcRole::Innkeeper, 4);
    m_npcs = {
        MakeNpc("npc.blacksmith", VillageNpcRole::Blacksmith, homePosition(blacksmithHome, 1.5f),
            Color{ 88, 79, 72, 255 }, blacksmithHome, 0.0f, 5.0f),
        MakeNpc("npc.mayor", VillageNpcRole::Mayor, homePosition(mayorHome),
                Color{ 47, 65, 98, 255 }, mayorHome, 5.0f, 2.1f),
        MakeNpc("npc.cleric", VillageNpcRole::Cleric, homePosition(clericHome),
                Color{ 211, 198, 166, 255 }, clericHome, 4.5f, 1.8f),
        MakeNpc("npc.farmer", VillageNpcRole::Farmer, homePosition(farmerHome, -1.5f),
            Color{ 82, 105, 54, 255 }, farmerHome, 7.0f, 3.0f),
        MakeNpc("npc.innkeeper", VillageNpcRole::Innkeeper, homePosition(innkeeperHome, 1.5f),
                Color{ 113, 66, 45, 255 }, innkeeperHome, 4.0f, 1.7f)
    };
    m_npcs[1].insideHome = true;
    m_npcs[2].insideHome = true;

    // Preserve quest and loot contents while binding each chest to its generated home.
    m_chests = {
        {blacksmithHome, {-2.5f, 0.0f, -2.5f}, 50, {{core::items::Shortsword, 1}}, false},
        {mayorHome, {3.0f, 0.0f, -2.0f}, 150, {{core::items::Longsword, 1}}, false},
        {clericHome, {-1.5f, 0.0f, -2.0f}, 0, {{core::items::Bow, 1}}, false},
        {innkeeperHome, {2.0f, 0.0f, 0.0f}, 10, {{core::items::Dagger, 1}}, false},
        {farmerHome, {-2.0f, 0.0f, 1.0f}, 5, {{core::items::Arrow, 20}}, false}
    };
}

void Village::SetModel(const std::string& asset, Model* model) {
    m_models[asset] = model;
}

void Village::SetNpcAssets(int index, Model* model, ModelAnimation* animations, int animationCount) {
    if (index < 0 || index >= static_cast<int>(m_npcs.size())) return;
    VillageNpc& npc = m_npcs[static_cast<std::size_t>(index)];
    npc.model = model;
    npc.animations = animations;
    npc.animationCount = animationCount;
}

void Village::HighlightBuilding(int index) {
    if (index < 0 || index >= static_cast<int>(m_buildings.size())) return;
    m_highlightedBuilding = index;
    m_highlightTimer = 0.55f;
}

void Village::Update(const World& world, NavigationSystem& navigation, float deltaTime, float timeOfDay) {
    m_highlightTimer = std::max(0.0f, m_highlightTimer - deltaTime);
    if (m_highlightTimer == 0.0f) m_highlightedBuilding = -1;
    const bool night = timeOfDay < 0.18f || timeOfDay > 0.82f;

    for (VillageNpc& npc : m_npcs) {
        const Vector3 homeDoor = npc.homeBuilding >= 0 &&
            npc.homeBuilding < static_cast<int>(m_buildings.size())
            ? DoorPosition(m_center, m_buildings[static_cast<std::size_t>(npc.homeBuilding)]) : m_center;
        npc.position.y = world.GetHeightAt(npc.position.x, npc.position.z);
        const bool daytimeIndoor = npc.role == VillageNpcRole::Mayor || npc.role == VillageNpcRole::Cleric ||
            (npc.role == VillageNpcRole::Innkeeper && timeOfDay >= 0.48f && timeOfDay < 0.66f);
        const bool wantsHome = night || daytimeIndoor;
        const auto setDestination = [&](Vector3 destination) {
            // Refresh occupied tiles excluding this NPC so it can leave its own tile.
            std::vector<Vector3> othersPos;
            othersPos.reserve(m_npcs.size());
            for (const VillageNpc& other : m_npcs) {
                if (&other != &npc) othersPos.push_back(other.position);
            }
            navigation.SetOccupiedTiles(othersPos);

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

        // Blacksmith stays at the anvil working — skip all routing.
        if (npc.role == VillageNpcRole::Blacksmith) {
            npc.walking = false;
        } else if (npc.insideHome) {
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
                            // Random wait between 1.5 and 4 seconds
                            std::uniform_real_distribution<float> waitDist(1.5f, 4.0f);
                            npc.waitTime = waitDist(NpcRng());
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
                if (!wantsHome && npc.waitTime <= 0.0f && npc.wanderRadius > 0.5f) {
                    // Pick a random destination within wanderRadius of spawnPos
                    std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159265f);
                    std::uniform_real_distribution<float> radiusDist(1.0f, npc.wanderRadius);
                    bool found = false;
                    for (int attempt = 0; attempt < 8; ++attempt) {
                        const float angle = angleDist(NpcRng());
                        const float r     = radiusDist(NpcRng());
                        const Vector3 dest{
                            npc.spawnPos.x + std::cos(angle) * r,
                            0.0f,
                            npc.spawnPos.z + std::sin(angle) * r
                        };
                        if (setDestination(dest)) { found = true; break; }
                    }
                    if (!found) npc.waitTime = 1.5f;
                } else if (wantsHome && !npc.returningHome) {
                    npc.returningHome = setDestination(homeDoor);
                }
            }
        }

        // Animation: clip 1 = walk, clip 0 = idle/work.
        // Never access a clip index >= animationCount.
        if (npc.model && npc.animations && npc.animationCount >= 1) {
            const bool isWorking = !night &&
                (npc.role == VillageNpcRole::Blacksmith || npc.role == VillageNpcRole::Farmer);
            int clip = 0; // default: idle / work
            if (npc.walking && npc.animationCount >= 2) {
                clip = 1; // walk clip
            }
            clip = std::clamp(clip, 0, npc.animationCount - 1);
            if (npc.appliedAnimation != clip) npc.animationTime = 0.0f;
            npc.animationTime += deltaTime;
            const float duration = isWorking ? 1.0f : (clip == 1 ? 1.0f : 2.4f);
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

Vector3 Village::FindNearestVillage(const World& world, Vector3 position) {
    return FindNearestVillage(world, position,
        { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(),
          std::numeric_limits<float>::quiet_NaN() });
}

Vector3 Village::FindNearestVillage(const World& world, Vector3 position, Vector3 excludedVillage) {
    const float regionSize = 128.0f;
    int centerRx = static_cast<int>(std::floor(position.x / regionSize));
    int centerRz = static_cast<int>(std::floor(position.z / regionSize));

    struct Cache {
        unsigned int seed = 0;
        int regionX = 0;
        int regionZ = 0;
        std::vector<Vector3> candidates;
        bool valid = false;
    };
    static Cache cache;
    if (!cache.valid || cache.seed != world.GetSeed() || cache.regionX != centerRx ||
        cache.regionZ != centerRz) {
        cache = { world.GetSeed(), centerRx, centerRz, {}, true };
        std::uniform_real_distribution<float> distOffset(24.0f, regionSize - 24.0f);
        for (int regionX = centerRx - 1; regionX <= centerRx + 1; ++regionX) {
            for (int regionZ = centerRz - 1; regionZ <= centerRz + 1; ++regionZ) {
                const std::uint32_t seed = world.GetSeed() ^
                    (static_cast<std::uint32_t>(regionX) * 73856093u) ^
                    (static_cast<std::uint32_t>(regionZ) * 19349663u);
                std::mt19937 rng(seed);
                for (int attempt = 0; attempt < 5; ++attempt) {
                    const float candidateX = std::floor(regionX * regionSize + distOffset(rng)) + 0.5f;
                    const float candidateZ = std::floor(regionZ * regionSize + distOffset(rng)) + 0.5f;
                    if (IsVillageSite(world, candidateX, candidateZ)) {
                        cache.candidates.push_back({ candidateX, world.GetHeightAt(candidateX, candidateZ),
                                                     candidateZ });
                        break;
                    }
                }
            }
        }
    }

    Vector3 nearest{};
    float minDistance = std::numeric_limits<float>::max();
    bool found = false;
    for (const Vector3 candidate : cache.candidates) {
        if (std::isfinite(excludedVillage.x) && std::isfinite(excludedVillage.z) &&
            std::fabs(candidate.x - excludedVillage.x) < 0.5f &&
            std::fabs(candidate.z - excludedVillage.z) < 0.5f) continue;
        const float dx = candidate.x - position.x;
        const float dz = candidate.z - position.z;
        const float distance = dx * dx + dz * dz;
        if (distance < minDistance) {
            minDistance = distance;
            nearest = candidate;
            found = true;
        }
    }
    return found ? nearest : FindSpawnPosition(world);
}

Vector3 Village::FindSpawnPosition(const World& world) {
    // Old logic: find the first valid site spiraling out from 0,0
    // We keep this to guarantee a spawn village exists.
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
    bounds.reserve(m_buildings.size() * 4 + m_instances.size());
    const auto addLocalObstacle = [&](const Building& building, float localX, float localZ,
                                      float width, float depth) {
        const Vector2 offset = RotateLocal(localX, localZ, building.yawDegrees);
        const float yaw = building.yawDegrees * DEG2RAD;
        const float halfX = std::fabs(std::cos(yaw)) * width * 0.5f +
            std::fabs(std::sin(yaw)) * depth * 0.5f;
        const float halfZ = std::fabs(std::sin(yaw)) * width * 0.5f +
            std::fabs(std::cos(yaw)) * depth * 0.5f;
        const float worldX = m_center.x + building.position.x + offset.x;
        const float worldZ = m_center.z + building.position.z + offset.y;
        bounds.push_back({ worldX - halfX, worldZ - halfZ, halfX * 2.0f, halfZ * 2.0f });
    };

    constexpr float wall = 0.30f;
    constexpr float doorWidth = 1.20f;
    for (const Building& building : m_buildings) {
        addLocalObstacle(building, -building.halfWidth + wall * 0.5f, 0.0f,
                         wall, building.halfDepth * 2.0f);
        addLocalObstacle(building, building.halfWidth - wall * 0.5f, 0.0f,
                         wall, building.halfDepth * 2.0f);
        addLocalObstacle(building, 0.0f, -building.halfDepth + wall * 0.5f,
                         building.halfWidth * 2.0f, wall);
        const float frontSegment = std::max(0.0f, building.halfWidth - doorWidth * 0.5f);
        if (frontSegment > 0.0f) {
            const float segmentWidth = frontSegment;
            const float segmentOffset = doorWidth * 0.5f + segmentWidth * 0.5f;
            addLocalObstacle(building, -segmentOffset, building.halfDepth - wall * 0.5f,
                             segmentWidth, wall);
            addLocalObstacle(building, segmentOffset, building.halfDepth - wall * 0.5f,
                             segmentWidth, wall);
        }
    }

    for (const Instance& instance : m_instances) {
        if (instance.buildingIndex >= 0 || instance.collisionRadius < 0.75f ||
            instance.asset.rfind("village/props/", 0) != 0) continue;
        const Vector2 offset = RotateLocal(instance.position.x, instance.position.z, 0.0f);
        const float radius = instance.collisionRadius * instance.scale;
        bounds.push_back({ m_center.x + offset.x - radius, m_center.z + offset.y - radius,
                           radius * 2.0f, radius * 2.0f });
    }
    return bounds;
}

Vector3 Village::BuildingDoorPosition(int buildingIndex) const {
    if (buildingIndex < 0 || buildingIndex >= static_cast<int>(m_buildings.size())) return {};
    return DoorPosition(m_center, m_buildings[static_cast<std::size_t>(buildingIndex)]);
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

InteriorChest* Village::ChestAtInteriorRay(Ray ray, int buildingIndex) {
    float closestDistance = 100000.0f;
    InteriorChest* foundChest = nullptr;
    for (auto& chest : m_chests) {
        if (chest.buildingIndex != buildingIndex) continue;
        const BoundingBox bounds = {
            { chest.position.x - 0.4f, chest.position.y, chest.position.z - 0.4f },
            { chest.position.x + 0.4f, chest.position.y + 0.6f, chest.position.z + 0.4f }
        };
        const RayCollision hit = GetRayCollisionBox(ray, bounds);
        if (hit.hit && hit.distance < closestDistance) {
            closestDistance = hit.distance;
            foundChest = &chest;
        }
    }
    return foundChest;
}

int Village::BuildingDoorAtTile(int tileX, int tileZ) const {
    for (int index = 0; index < static_cast<int>(m_buildings.size()); ++index) {
        const Building& building = m_buildings[static_cast<std::size_t>(index)];
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
    for (int index = 0; index < static_cast<int>(m_buildings.size()); ++index) {
        const Building& building = m_buildings[static_cast<std::size_t>(index)];
        const float yaw = building.yawDegrees * DEG2RAD;
        const float halfX = std::fabs(std::cos(yaw)) * building.halfWidth +
            std::fabs(std::sin(yaw)) * building.halfDepth;
        const float halfZ = std::fabs(std::sin(yaw)) * building.halfWidth +
            std::fabs(std::cos(yaw)) * building.halfDepth;
        const float x = m_center.x + building.position.x;
        const float z = m_center.z + building.position.z;
        if (!IsChunkVisible(world, { x, 0.0f, z }, ray.position)) continue;
        const float ground = BuildingGround(world, x, z, halfX, halfZ);
        const Vector3 door = DoorPosition(m_center, building);
        const Vector2 faceOffset = RotateLocal(building.doorStep.x, building.doorStep.z - 0.65f,
                                              building.yawDegrees);
        const Vector3 doorFace{ m_center.x + building.position.x + faceOffset.x,
                                ground + 1.47f,
                                m_center.z + building.position.z + faceOffset.y };
        const float doorHalfX = std::fabs(std::cos(yaw)) * 0.60f + std::fabs(std::sin(yaw)) * 0.16f;
        const float doorHalfZ = std::fabs(std::sin(yaw)) * 0.60f + std::fabs(std::cos(yaw)) * 0.16f;
        const BoundingBox doorBounds = {
            { doorFace.x - doorHalfX, ground + 0.42f, doorFace.z - doorHalfZ },
            { doorFace.x + doorHalfX, ground + 2.52f, doorFace.z + doorHalfZ }
        };

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

std::string Village::BuildingNameKey(int buildingIndex) const {
    if (buildingIndex < 0 || buildingIndex >= static_cast<int>(m_buildings.size()))
        return "village.building.home";
    const std::string& kind = m_buildings[static_cast<std::size_t>(buildingIndex)].kind;
    if (kind == "forge") return "village.building.forge";
    if (kind == "hall") return "village.building.hall";
    if (kind == "chapel") return "village.building.chapel";
    if (kind == "inn") return "village.building.inn";
    if (kind == "farmhouse") return "village.building.farmhouse";
    return "village.building.home";
}

std::string Village::BuildingKind(int buildingIndex) const {
    if (buildingIndex < 0 || buildingIndex >= static_cast<int>(m_buildings.size())) return {};
    return m_buildings[static_cast<std::size_t>(buildingIndex)].kind;
}

void Village::Render(const World& world, Vector3 playerPosition) const {
    for (const RoadRun& run : m_roadRuns) {
        int segmentLength = 0;
        float segmentGround = 0.0f;
        const auto flushSegment = [&]() {
            if (segmentLength == 0) return;
            const float offset = (segmentLength - 1) * 0.5f;
            const Vector3 position{
                run.start.x + (run.alongX ? offset : 0.0f),
                segmentGround + 0.035f,
                run.start.y + (run.alongX ? 0.0f : offset)
            };
            const float length = segmentLength * 0.98f;
            DrawCube(position, run.alongX ? length : 0.98f, 0.07f,
                     run.alongX ? 0.98f : length, Color{ 128, 116, 96, 255 });
            segmentLength = 0;
        };
        for (int tileIndex = 0; tileIndex <= run.tileCount; ++tileIndex) {
            const Vector3 tile{
                run.start.x + (run.alongX ? static_cast<float>(tileIndex) : 0.0f), 0.0f,
                run.start.y + (run.alongX ? 0.0f : static_cast<float>(tileIndex))
            };
            const bool visible = tileIndex < run.tileCount &&
                IsChunkVisible(world, tile, playerPosition) && !world.IsWaterAt(tile.x, tile.z);
            const float ground = visible ? world.GetHeightAt(tile.x, tile.z) : 0.0f;
            if (!visible || (segmentLength > 0 && std::fabs(ground - segmentGround) > 0.001f))
                flushSegment();
            if (visible) {
                if (segmentLength == 0) segmentGround = ground;
                ++segmentLength;
            }
        }
    }

    for (const Instance& instance : m_instances) {
        const float x = m_center.x + instance.position.x;
        const float z = m_center.z + instance.position.z;
        const Vector3 worldPosition{ x, 0.0f, z };
        if (!IsChunkVisible(world, worldPosition, playerPosition) || world.IsWaterAt(x, z)) continue;
        const auto modelEntry = m_models.find(instance.asset);
        if (modelEntry == m_models.end() || !modelEntry->second || modelEntry->second->meshCount == 0) continue;

        float ground = world.GetHeightAt(x, z);
        if (instance.buildingIndex >= 0) {
            const Building& building = m_buildings[static_cast<std::size_t>(instance.buildingIndex)];
            const float yaw = building.yawDegrees * DEG2RAD;
            const float halfX = std::fabs(std::cos(yaw)) * building.halfWidth +
                std::fabs(std::sin(yaw)) * building.halfDepth;
            const float halfZ = std::fabs(std::sin(yaw)) * building.halfWidth +
                std::fabs(std::cos(yaw)) * building.halfDepth;
            ground = BuildingGround(world, x, z, halfX, halfZ);
        }
        const Color tint = instance.buildingIndex == m_highlightedBuilding && m_highlightTimer > 0.0f
            ? Color{ 255, 218, 102, 255 } : WHITE;
        DrawModelEx(*modelEntry->second, { x, ground + instance.position.y, z },
                    { 0.0f, 1.0f, 0.0f }, instance.yawDegrees,
                    { instance.scale, instance.scale, instance.scale }, tint);
    }

    for (const VillageNpc& npc : m_npcs) {
        if (!npc.insideHome && IsChunkVisible(world, npc.position, playerPosition)) {
            Vector3 position = npc.position;
            position.y = world.GetHeightAt(position.x, position.z) + 0.04f;
            DrawResident(npc, position);
        }
    }
}

void Village::RenderInterior(int buildingIndex, Vector3 cameraPosition, Vector3 playerPosition,
                             Ray hoverRay) const {
    const std::string buildingKind = buildingIndex >= 0 &&
        buildingIndex < static_cast<int>(m_buildings.size())
        ? m_buildings[static_cast<std::size_t>(buildingIndex)].kind : std::string{};
    const Color timber{ 83, 58, 39, 255 };
    const Color plaster{ 142, 115, 82, 255 };
    const Color floor{ 105, 80, 58, 255 };
    const Vector3 viewVector = Vector3Subtract(playerPosition, cameraPosition);
    const float viewDistance = Vector3Length(viewVector);
    const Ray viewRay = { cameraPosition, viewDistance > 0.001f
        ? Vector3Scale(viewVector, 1.0f / viewDistance) : Vector3{ 0.0f, 0.0f, 0.0f } };
    const auto drawInteriorCube = [&](Vector3 position, float width, float height, float depth, Color color) {
        const BoundingBox bounds = {
            { position.x - width * 0.5f, position.y - height * 0.5f, position.z - depth * 0.5f },
            { position.x + width * 0.5f, position.y + height * 0.5f, position.z + depth * 0.5f }
        };
        // Thick raycast approximation: check if object is in front of player (z > player.z)
        // AND within a horizontal corridor to the camera. Since camera is at +Z looking -Z,
        // we check if the object's X bounds overlap the player's X (with some margin) 
        // and its Z is greater than the player's Z.
        bool occludesPlayer = false;
        if (viewDistance > 0.001f) {
            if (bounds.min.z > playerPosition.z - 0.2f && 
                bounds.min.y < playerPosition.y + 2.0f &&
                bounds.max.y > playerPosition.y) {
                // Check X overlap with a 1.0f wide corridor around the player
                if (bounds.max.x > playerPosition.x - 1.0f && bounds.min.x < playerPosition.x + 1.0f) {
                    occludesPlayer = true;
                }
            }
            // Also keep the exact raycast as fallback for edge cases
            if (!occludesPlayer) {
                const RayCollision hit = GetRayCollisionBox(viewRay, bounds);
                occludesPlayer = hit.hit && hit.distance < viewDistance - 0.18f;
            }
        }

        if (occludesPlayer) {
            BeginBlendMode(BLEND_ALPHA);
            rlDisableDepthMask();
            DrawCube(position, width, height, depth, Fade(color, 0.18f));
            rlEnableDepthMask();
            EndBlendMode();
        } else {
            DrawCube(position, width, height, depth, color);
        }
    };

    drawInteriorCube({ 0.0f, -0.18f, 0.0f }, 10.0f, 0.36f, 9.0f, floor);
    drawInteriorCube({ 0.0f, 1.5f, -4.35f }, 10.0f, 3.0f, 0.3f, plaster);
    drawInteriorCube({ -4.85f, 1.5f, 0.0f }, 0.3f, 3.0f, 9.0f, plaster);
    drawInteriorCube({ 4.85f, 1.5f, 0.0f }, 0.3f, 3.0f, 9.0f, plaster);
    drawInteriorCube({ -2.65f, 1.5f, 4.35f }, 4.7f, 3.0f, 0.3f, plaster);
    drawInteriorCube({ 2.65f, 1.5f, 4.35f }, 4.7f, 3.0f, 0.3f, plaster);
    drawInteriorCube({ 0.0f, 2.95f, 4.35f }, 1.0f, 0.2f, 0.3f, timber);

    constexpr float gridCellSize = 1.0f;
    for (int cellX = 0; cellX < 10; ++cellX) {
        for (int cellZ = 0; cellZ < 9; ++cellZ) {
            const float x = -4.5f + static_cast<float>(cellX);
            const float z = -4.0f + static_cast<float>(cellZ);
            DrawCubeWires({ x, 0.005f, z }, gridCellSize * 0.96f, 0.005f,
                          gridCellSize * 0.96f, Color{ 75, 55, 35, 140 });
        }
    }

    if (std::fabs(hoverRay.direction.y) > 0.0001f) {
        const float floorDistance = -hoverRay.position.y / hoverRay.direction.y;
        if (floorDistance > 0.0f) {
            const float hitX = hoverRay.position.x + hoverRay.direction.x * floorDistance;
            const float hitZ = hoverRay.position.z + hoverRay.direction.z * floorDistance;
            const int cellX = static_cast<int>(std::floor(hitX + 5.0f));
            const int cellZ = static_cast<int>(std::floor(hitZ + 4.5f));
            if (cellX >= 0 && cellX < 10 && cellZ >= 0 && cellZ < 9) {
                DrawCube({ -4.5f + cellX, 0.025f, -4.0f + cellZ }, 0.92f, 0.025f, 0.92f,
                          Color{ 224, 184, 92, 115 });
            }
        }
    }

    if (buildingKind == "forge") {
        drawInteriorCube({ -3.0f, 0.55f, -2.0f }, 2.0f, 1.1f, 1.3f, Color{ 65, 57, 50, 255 });
        drawInteriorCube({ -3.0f, 1.2f, -2.0f }, 1.0f, 0.35f, 0.6f, ORANGE);
        drawInteriorCube({ 1.0f, 0.52f, 0.0f }, 2.4f, 1.0f, 1.0f, timber);
        drawInteriorCube({ 1.0f, 1.08f, 0.0f }, 0.25f, 0.12f, 0.25f, LIGHTGRAY);
    } else if (buildingKind == "hall") {
        drawInteriorCube({ 0.0f, 0.62f, -1.6f }, 3.8f, 1.2f, 1.25f, timber);
        drawInteriorCube({ -3.4f, 1.45f, -2.7f }, 0.35f, 2.5f, 1.3f, DARKBROWN);
        drawInteriorCube({ -3.4f, 1.9f, -1.7f }, 0.35f, 0.08f, 1.3f, GOLD);
        drawInteriorCube({ 3.4f, 1.45f, -2.7f }, 0.35f, 2.5f, 1.3f, DARKBROWN);
    } else if (buildingKind == "chapel") {
        drawInteriorCube({ 0.0f, 0.42f, -2.7f }, 2.6f, 0.8f, 1.0f, Color{ 174, 156, 119, 255 });
        drawInteriorCube({ 0.0f, 1.2f, -2.7f }, 0.12f, 1.0f, 0.12f, GOLD);
        drawInteriorCube({ 0.0f, 1.45f, -2.7f }, 0.5f, 0.12f, 0.12f, GOLD);
        DrawCandle({ -1.0f, 0.85f, -2.7f }, 0.12f, 0.35f, 8, Color{ 240, 174, 65, 255 });
        DrawCandle({ 1.0f, 0.85f, -2.7f }, 0.12f, 0.35f, 8, Color{ 240, 174, 65, 255 });
    } else {
        drawInteriorCube({ 0.0f, 0.65f, -3.0f }, 6.6f, 1.3f, 0.8f, Color{ 105, 67, 42, 255 });
        drawInteriorCube({ -2.7f, 0.35f, 0.0f }, 1.5f, 0.7f, 1.3f, timber);
        drawInteriorCube({ 2.7f, 0.35f, 0.0f }, 1.5f, 0.7f, 1.3f, timber);
        drawInteriorCube({ 0.0f, 0.34f, 1.7f }, 2.0f, 0.68f, 1.1f, timber);
    }

    int interiorNpcIndex = 0;
    for (const VillageNpc& npc : m_npcs) {
        if (!npc.insideHome || npc.homeBuilding != buildingIndex) continue;
        const Vector3 position{ -2.6f + (interiorNpcIndex % 2) * 1.1f, 0.0f,
                                -0.5f + (interiorNpcIndex / 2) * 1.2f };
        DrawResident(npc, position);
        ++interiorNpcIndex;
    }

    // Render Chests
    for (const auto& chest : m_chests) {
        if (chest.buildingIndex != buildingIndex) continue;
        Color chestColor = chest.looted ? Color{ 100, 80, 60, 255 } : Color{ 180, 120, 40, 255 };
        drawInteriorCube({ chest.position.x, chest.position.y + 0.3f, chest.position.z }, 0.8f, 0.6f, 0.6f, chestColor);
        // Lock / trim detail
        drawInteriorCube({ chest.position.x, chest.position.y + 0.3f, chest.position.z + 0.31f }, 0.2f, 0.2f, 0.05f, GOLD);
    }
}
