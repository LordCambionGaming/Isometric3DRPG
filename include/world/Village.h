#pragma once

#include <raylib.h>
#include <array>
#include <string>
#include <unordered_map>
#include <vector>
#include "core/Items.h"

class World;
class NavigationSystem;

enum class VillageNpcRole {
    Blacksmith,
    Mayor,
    Cleric,
    Farmer,
    Innkeeper
};

struct VillageNpc {
    std::string id;
    VillageNpcRole role;
    Vector3 villageCenter{};
    Vector3 position;
    Vector3 spawnPos{};       // origin for random wandering
    Vector3 target{};
    Color clothing;
    int homeBuilding = 0;
    std::array<Vector2, 3> route{};
    std::vector<Vector3> waypoints;
    int routeCount = 0;
    int routeIndex = 0;
    std::size_t waypointIndex = 0;
    float waitTime = 0.0f;
    float animationTime = 0.0f;
    float oneShotTime = 0.0f;
    float facing = 0.0f;
    float wanderRadius = 6.0f; // max wander distance from spawnPos
    bool walking = false;
    bool insideHome = false;
    bool returningHome = false;
    Model* model = nullptr;
    ModelAnimation* animations = nullptr;
    int animationCount = 0;
    int appliedAnimation = -1;

    int appliedFrame = -1;
};

struct InteriorChest {
    int buildingIndex;
    Vector3 position;
    int gold;
    std::vector<core::ItemStack> items;
    bool looted;
};

class Village {
public:
    explicit Village(Vector3 center);

    static Vector3 FindSpawnPosition(const World& world);
    static Vector3 FindNearestVillage(const World& world, Vector3 position);
    static Vector3 FindNearestVillage(const World& world, Vector3 position, Vector3 excludedVillage);
    bool EnsureVillagesNear(const World& world, Vector3 position);
    bool PruneDistantVillages(Vector3 position, float maxDistance);
    Vector3 NearestVillageCenter(Vector3 position) const;
    Vector3 CenterPosition() const { return m_center; }
    const std::vector<Vector3>& GetVillageCenters() const { return m_villageCenters; }
    int BuildingCount() const { return static_cast<int>(m_buildings.size()); }
    void Render(const World& world, Vector3 playerPosition) const;
    void RenderInterior(int buildingIndex, Vector3 cameraPosition, Vector3 playerPosition,
                        Ray hoverRay) const;
    static void SetCubeShader(Shader shader);
    static void UnloadResources();
    // Chests
    std::vector<InteriorChest>& GetChests() { return m_chests; }
    InteriorChest* ChestAtInteriorRay(Ray ray, int buildingIndex);
    void Update(const World& world, NavigationSystem& navigation, Vector3 playerPosition,
                float deltaTime, float timeOfDay);
    const std::vector<std::string>& RequiredModelAssets() const { return m_modelAssets; }
    void SetModel(const std::string& asset, Model* model);
    void SetNpcAssets(int index, Model* model, ModelAnimation* animations, int animationCount);
    void HighlightBuilding(int index);
    std::vector<Rectangle> CollisionBounds() const;
    const VillageNpc* NpcAtTile(int tileX, int tileZ) const;
    const VillageNpc* NpcAtRay(const World& world, Ray ray) const;
    bool NpcAtInteriorRay(Ray ray, int buildingIndex, VillageNpc& outNpc) const;
    int BuildingDoorAtTile(int tileX, int tileZ) const;
    int BuildingDoorAtRay(const World& world, Ray ray) const;
    Vector3 BuildingDoorPosition(int buildingIndex) const;
    std::vector<Vector3> NpcPositions() const;
    std::vector<Vector3> NpcPositions(Vector3 center, float maxDistance) const;
    int NpcCount() const { return static_cast<int>(m_npcs.size()); }
    std::string BuildingNameKey(int buildingIndex) const;
    std::string BuildingKind(int buildingIndex) const;

private:
    struct Building {
        std::string kind;
        Vector3 center{};
        Vector3 position{};
        float yawDegrees = 0.0f;
        float halfWidth = 0.0f;
        float halfDepth = 0.0f;
        Vector3 doorStep{};
        std::vector<Vector2> roadPath;
        int instanceIndex = -1;
    };

    struct Instance {
        std::string asset;
        Vector3 center{};
        Vector3 position{};
        float yawDegrees = 0.0f;
        float scale = 1.0f;
        float collisionRadius = 0.0f;
        int buildingIndex = -1;
    };

    struct RoadRun {
        Vector2 start{};
        int tileCount = 0;
        bool alongX = true;
    };

    bool AddVillageLayout(Vector3 center, float yawDegrees, unsigned int variantSeed = 0);
    void RebuildRoadRuns();
    void ApplyNpcAssets(VillageNpc& npc);

    Vector3 m_center;
    std::vector<Vector3> m_villageCenters;
    std::vector<Building> m_buildings;
    std::vector<Instance> m_instances;
    std::vector<RoadRun> m_roadRuns;
    std::vector<std::string> m_modelAssets;
    std::unordered_map<std::string, Model*> m_models;
    std::array<Model*, 5> m_npcModels{};
    std::array<ModelAnimation*, 5> m_npcAnimations{};
    std::array<int, 5> m_npcAnimationCounts{};
    std::vector<VillageNpc> m_npcs;
    std::vector<InteriorChest> m_chests;
    int m_highlightedBuilding = -1;
    float m_highlightTimer = 0.0f;
};
