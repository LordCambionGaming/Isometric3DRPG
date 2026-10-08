#pragma once

#include <raylib.h>
#include <array>
#include <string>
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
    Vector3 position;
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
    void Render(const World& world) const;
    void RenderInterior(int buildingIndex, Vector3 cameraPosition, Vector3 playerPosition) const;
    
    // Chests
    std::vector<InteriorChest>& GetChests() { return m_chests; }
    InteriorChest* ChestAtInteriorRay(Ray ray, int buildingIndex);
    void Update(const World& world, NavigationSystem& navigation, float deltaTime, float timeOfDay);
    void SetBuildingModel(int index, Model* model);
    void SetNpcAssets(int index, Model* model, ModelAnimation* animations, int animationCount);
    void HighlightBuilding(int index);
    std::vector<Rectangle> CollisionBounds() const;
    const VillageNpc* NpcAtTile(int tileX, int tileZ) const;
    const VillageNpc* NpcAtRay(Ray ray) const;
    bool NpcAtInteriorRay(Ray ray, int buildingIndex, VillageNpc& outNpc) const;
    int BuildingDoorAtTile(int tileX, int tileZ) const;
    int BuildingDoorAtRay(const World& world, Ray ray) const;
    Vector3 BuildingDoorPosition(int buildingIndex) const;
    std::vector<Vector3> NpcPositions() const;
    int NpcCount() const { return static_cast<int>(m_npcs.size()); }
    static std::string BuildingNameKey(int buildingIndex);

private:
    Vector3 m_center;
    std::vector<VillageNpc> m_npcs;
    std::vector<InteriorChest> m_chests;
    std::array<Model*, 5> m_buildingModels{};
    int m_highlightedBuilding = -1;
    float m_highlightTimer = 0.0f;
};
