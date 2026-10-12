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
#include <map>
#include <optional>
#include <random>
#include <tuple>
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

bool IsVillageSite(const World& world, float centerX, float centerZ, float villageYawDegrees = 0.0f) {
    float minHeight = 100000.0f;
    float maxHeight = -100000.0f;
    auto sample = [&](float x, float z) {
        if (world.IsWaterAt(x, z)) return false;
        float height = world.GetHeightAt(x, z);
        if (height == 0.0f) height = 2.0f;
        minHeight = std::min(minHeight, height);
        maxHeight = std::max(maxHeight, height);
        return true;
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
        if (!instance.contains("kind") ||!instance.contains("pos") ||!instance["pos"].is_array()) continue;
        const float villageYaw = villageYawDegrees * DEG2RAD;
        const float localX = instance["pos"][0].get<float>();
        const float localZ = instance["pos"][2].get<float>();
        const float houseX = centerX + localX * std::cos(villageYaw) + localZ * std::sin(villageYaw);
        const float houseZ = centerZ - localX * std::sin(villageYaw) + localZ * std::cos(villageYaw);
        const float halfWidth = instance.value("half_width", 2.5f);
        const float halfDepth = instance.value("half_depth", 2.0f);
        const float yaw = (instance.value("yaw_deg", 0.0f) + villageYawDegrees) * DEG2RAD;
        for (int lz = -static_cast<int>(std::ceil(halfDepth)); lz <= static_cast<int>(std::ceil(halfDepth)); ++lz) {
            for (int lx = -static_cast<int>(std::ceil(halfWidth)); lx <= static_cast<int>(std::ceil(halfWidth)); ++lx) {
                const float x = houseX + lx * std::cos(yaw) + lz * std::sin(yaw);
                const float z = houseZ - lx * std::sin(yaw) + lz * std::cos(yaw);
                if (!sample(x, z)) return false;
            }
        }
    }
     if (maxHeight - minHeight > 1.2f) return false;

    // Entroterra: gli anelli a 45 e 70 blocchi devono essere asciutti,
    // così il villaggio non nasce su un'isola o una penisola sottile.
    for (float radius : { 45.0f, 70.0f }) {
        for (int i = 0; i < 16; ++i) {
            const float a = i * (2.0f * PI / 16.0f);
            if (world.IsWaterAt(centerX + std::cos(a) * radius, centerZ + std::sin(a) * radius)) return false;
        }
    }
    return true;
    return maxHeight - minHeight <= 1.2f;
} 
std::optional<Vector3> FindVillageSiteForRegion(const World& world, int regionX, int regionZ) {
    constexpr float regionSize = 192.0f;
    using RegionKey = std::tuple<unsigned int, int, int>;
    static std::map<RegionKey, std::optional<Vector3>> siteCache;
    const RegionKey key{ world.GetSeed(), regionX, regionZ };
    if (const auto cached = siteCache.find(key); cached!= siteCache.end()) return cached->second;

    const std::uint32_t seed = world.GetSeed() ^ (static_cast<std::uint32_t>(regionX) * 73856093u) ^ (static_cast<std::uint32_t>(regionZ) * 19349663u);
    const float villageYaw = static_cast<float>((seed >> 2) % 4u) * 90.0f;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> offset(20.0f, 172.0f); // FIX: cerca in tutta la regione, non solo centro
    for (int attempt = 0; attempt < 32; ++attempt) { // FIX: 32 tentativi non 8
        const float x = std::floor(regionX * regionSize + offset(rng)) + 0.5f;
        const float z = std::floor(regionZ * regionSize + offset(rng)) + 0.5f;
        if (IsVillageSite(world, x, z, villageYaw)) {
            const std::optional<Vector3> site = Vector3{ x, world.GetHeightAt(x, z), z };
            if (siteCache.size() > 4096) siteCache.clear();
            siteCache.emplace(key, site);
            return site;
        }
    }
    siteCache.emplace(key, std::nullopt);
    if (siteCache.size() > 4096) siteCache.clear();
    return std::nullopt;
}

void DrawResident(const VillageNpc& npc, Vector3 position) {
    if (!npc.model || npc.model->meshCount == 0) return;
    DrawModelEx(*npc.model, position, { 0.0f, 1.0f, 0.0f }, npc.facing, { 1.0f, 1.0f, 1.0f }, WHITE);
}
VillageNpc MakeNpc(const char* id, VillageNpcRole role, Vector3 villageCenter, Vector3 position, Color clothing, int homeBuilding, float wanderRadius, float waitTime) {
    VillageNpc npc; npc.id=id; npc.role=role; npc.villageCenter=villageCenter; npc.position=position; npc.spawnPos=position; npc.clothing=clothing; npc.homeBuilding=homeBuilding; npc.wanderRadius=wanderRadius; npc.target=position; npc.waitTime=waitTime; return npc;
}
Vector2 RotateLocal(float x, float z, float yawDegrees) {
    const float yaw = yawDegrees * DEG2RAD;
    return { x * std::cos(yaw) + z * std::sin(yaw), -x * std::sin(yaw) + z * std::cos(yaw) };
}
template <typename BuildingType> Vector3 DoorPosition(const BuildingType& building) {
    const Vector2 offset = RotateLocal(building.doorStep.x, building.doorStep.z, building.yawDegrees);
    return { building.center.x + building.position.x + offset.x, building.center.y + building.doorStep.y, building.center.z + building.position.z + offset.y };
}
bool IsChunkVisible(const World& world, Vector3 position, Vector3 playerPosition) {
    int chunkX=0, chunkZ=0, playerChunkX=0, playerChunkZ=0;
    world.WorldToChunk(position.x, position.z, chunkX, chunkZ);
    world.WorldToChunk(playerPosition.x, playerPosition.z, playerChunkX, playerChunkZ);
    return world.IsChunkLoaded(chunkX, chunkZ) && std::abs(chunkX-playerChunkX)<=world.GetRenderRadius() && std::abs(chunkZ-playerChunkZ)<=world.GetRenderRadius();
}
std::string BuildingKindForRole(VillageNpcRole role) {
    switch(role){case VillageNpcRole::Blacksmith:return "forge";case VillageNpcRole::Mayor:return "hall";case VillageNpcRole::Cleric:return "chapel";case VillageNpcRole::Farmer:return "farmhouse";case VillageNpcRole::Innkeeper:return "inn";} return {};
}
void DrawCandle(Vector3 position, float radius, float height, int slices, Color flameColor) {
    DrawCylinder(position, radius, radius, height, slices, Color{218,203,164,255});
    DrawSphere({position.x, position.y+height+radius, position.z}, radius*1.15f, flameColor);
}

Shader g_cubeShader{};
bool   g_hasCubeShader = false;

// Cubo con normali vere, al posto di DrawCube (che non le manda allo shader)
void DrawLitCube(Vector3 position, float width, float height, float depth, Color color) {
    static Model cube = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
    if (g_hasCubeShader) cube.materials[0].shader = g_cubeShader;
    DrawModelEx(cube, position, Vector3{ 0.0f, 1.0f, 0.0f }, 0.0f, Vector3{ width, height, depth }, color);
}
 

std::mt19937& NpcRng(){ static thread_local std::mt19937 rng(std::random_device{}()); return rng; }
} // namespace
void Village::SetCubeShader(Shader shader) { g_cubeShader = shader; g_hasCubeShader = true; }
Village::Village(Vector3 center) : m_center(center) { AddVillageLayout(center, 0.0f, 0u); RebuildRoadRuns(); }

bool Village::AddVillageLayout(Vector3 center, float villageYawDegrees, unsigned int variantSeed) {
    for (const Vector3 existing : m_villageCenters) if (std::fabs(existing.x-center.x)<0.5f && std::fabs(existing.z-center.z)<0.5f) return false;
    const nlohmann::json& layout=VillageLayout(); const std::size_t buildingStart=m_buildings.size(); const std::size_t npcStart=m_npcs.size();
    const auto rotatePosition=[villageYawDegrees](Vector3 position){const Vector2 rotated=RotateLocal(position.x,position.z,villageYawDegrees); return Vector3{rotated.x,position.y,rotated.y};};
    if (layout.contains("instances") && layout["instances"].is_array()) {
        for (const auto& data : layout["instances"]) {
            if (!data.contains("asset")||!data["asset"].is_string()||!data.contains("pos")||!data["pos"].is_array()||data["pos"].size()<3) continue;
            Instance instance; instance.asset=data["asset"].get<std::string>();
            if (data.contains("kind")&&data["kind"].is_string()){const std::string kind=data["kind"].get<std::string>(); const std::string baseHouseAsset="village/"+kind+".glb"; if((variantSeed&1u)!=0u) instance.asset=baseHouseAsset; if(std::find(m_modelAssets.begin(),m_modelAssets.end(),baseHouseAsset)==m_modelAssets.end()) m_modelAssets.push_back(baseHouseAsset);}
            instance.center=center; instance.position=rotatePosition({data["pos"][0].get<float>(),data["pos"][1].get<float>(),data["pos"][2].get<float>()}); instance.yawDegrees=data.value("yaw_deg",0.0f)+villageYawDegrees; instance.scale=data.value("scale",1.0f); instance.collisionRadius=data.value("collision_radius",0.0f);
            if(std::find(m_modelAssets.begin(),m_modelAssets.end(),instance.asset)==m_modelAssets.end()) m_modelAssets.push_back(instance.asset);
            if (data.contains("kind")&&data["kind"].is_string()){Building building; building.kind=data["kind"].get<std::string>(); building.center=center; building.position=instance.position; building.yawDegrees=instance.yawDegrees; building.halfWidth=data.value("half_width",2.5f); building.halfDepth=data.value("half_depth",2.0f); if(data.contains("door_step_pos")&&data["door_step_pos"].is_array()&&data["door_step_pos"].size()>=3) building.doorStep={data["door_step_pos"][0].get<float>(),data["door_step_pos"][1].get<float>(),data["door_step_pos"][2].get<float>()}; if(data.contains("road_path")&&data["road_path"].is_array()){for(const auto& point:data["road_path"]){if(point.is_array()&&point.size()>=2){const Vector2 roadPoint=RotateLocal(point[0].get<float>(),point[1].get<float>(),villageYawDegrees); building.roadPath.push_back(roadPoint);}}} building.instanceIndex=static_cast<int>(m_instances.size()); instance.buildingIndex=static_cast<int>(m_buildings.size()); m_buildings.push_back(std::move(building));}
            m_instances.push_back(std::move(instance));
        }
    }
    const auto homeFor=[&](VillageNpcRole role,int fallback){const std::string kind=BuildingKindForRole(role); for(int index=static_cast<int>(buildingStart);index<static_cast<int>(m_buildings.size());++index){if(m_buildings[static_cast<std::size_t>(index)].kind==kind) return index;} const int buildingCount=static_cast<int>(m_buildings.size())-static_cast<int>(buildingStart); return buildingCount==0?-1:static_cast<int>(buildingStart)+fallback%buildingCount;};
    const auto homePosition=[&](int index,float lateralOffset=0.0f){if(index<0||index>=static_cast<int>(m_buildings.size())) return center; const Building& building=m_buildings[static_cast<std::size_t>(index)]; const Vector2 offset=RotateLocal(lateralOffset,building.doorStep.z,building.yawDegrees); return Vector3{building.center.x+building.position.x+offset.x,building.center.y+building.doorStep.y,building.center.z+building.position.z+offset.y};};
    const int blacksmithHome=homeFor(VillageNpcRole::Blacksmith,0); const int mayorHome=homeFor(VillageNpcRole::Mayor,1); const int clericHome=homeFor(VillageNpcRole::Cleric,2); const int farmerHome=homeFor(VillageNpcRole::Farmer,3); const int innkeeperHome=homeFor(VillageNpcRole::Innkeeper,4);
    const std::array<VillageNpc,5> residents={{MakeNpc("npc.blacksmith",VillageNpcRole::Blacksmith,center,homePosition(blacksmithHome,1.5f),Color{88,79,72,255},blacksmithHome,0.0f,5.0f),MakeNpc("npc.mayor",VillageNpcRole::Mayor,center,homePosition(mayorHome),Color{47,65,98,255},mayorHome,5.0f,2.1f),MakeNpc("npc.cleric",VillageNpcRole::Cleric,center,homePosition(clericHome),Color{211,198,166,255},clericHome,4.5f,1.8f),MakeNpc("npc.farmer",VillageNpcRole::Farmer,center,homePosition(farmerHome,-1.5f),Color{82,105,54,255},farmerHome,7.0f,3.0f),MakeNpc("npc.innkeeper",VillageNpcRole::Innkeeper,center,homePosition(innkeeperHome,1.5f),Color{113,66,45,255},innkeeperHome,4.0f,1.7f)}};
    m_npcs.insert(m_npcs.end(),residents.begin(),residents.end()); m_npcs[npcStart+1].insideHome=true; m_npcs[npcStart+2].insideHome=true; for(std::size_t index=npcStart;index<m_npcs.size();++index) ApplyNpcAssets(m_npcs[index]);
    const std::array<InteriorChest,5> chests={{ {blacksmithHome,{-2.5f,0.0f,-2.5f},50,{{core::items::Shortsword,1}},false},{mayorHome,{3.0f,0.0f,-2.0f},150,{{core::items::Longsword,1}},false},{clericHome,{-1.5f,0.0f,-2.0f},0,{{core::items::Bow,1}},false},{innkeeperHome,{2.0f,0.0f,0.0f},10,{{core::items::Dagger,1}},false},{farmerHome,{-2.0f,0.0f,1.0f},5,{{core::items::Arrow,20}},false} }};
    m_chests.insert(m_chests.end(),chests.begin(),chests.end()); m_villageCenters.push_back(center); return m_buildings.size()>buildingStart;
}
void Village::RebuildRoadRuns(){m_roadRuns.clear(); std::unordered_set<std::int64_t> remainingRoadTiles; for(const Building& building:m_buildings){for(std::size_t segment=1;segment<building.roadPath.size();++segment){const Vector2 from=building.roadPath[segment-1]; const Vector2 to=building.roadPath[segment]; const float distance=std::fabs(to.x-from.x)+std::fabs(to.y-from.y); const int steps=std::max(1,static_cast<int>(std::ceil(distance))); for(int step=0;step<=steps;++step){const float amount=static_cast<float>(step)/static_cast<float>(steps); const int tileX=static_cast<int>(std::floor(building.center.x+from.x+(to.x-from.x)*amount)); const int tileZ=static_cast<int>(std::floor(building.center.z+from.y+(to.y-from.y)*amount)); remainingRoadTiles.insert((static_cast<std::int64_t>(static_cast<std::uint32_t>(tileX))<<32)|static_cast<std::uint32_t>(tileZ));}}} std::vector<std::pair<int,int>> orderedRoadTiles; orderedRoadTiles.reserve(remainingRoadTiles.size()); for(const std::int64_t key:remainingRoadTiles) orderedRoadTiles.emplace_back(static_cast<std::int32_t>(static_cast<std::uint64_t>(key)>>32),static_cast<std::int32_t>(static_cast<std::uint32_t>(key))); std::sort(orderedRoadTiles.begin(),orderedRoadTiles.end(),[](const auto& left,const auto& right){return left.second==right.second?left.first<right.first:left.second<right.second;}); const auto roadKey=[](int tileX,int tileZ){return (static_cast<std::int64_t>(static_cast<std::uint32_t>(tileX))<<32)|static_cast<std::uint32_t>(tileZ);}; for(const auto& tile:orderedRoadTiles){if(remainingRoadTiles.erase(roadKey(tile.first,tile.second))==0) continue; int tileCount=1; while(remainingRoadTiles.erase(roadKey(tile.first+tileCount,tile.second))>0) ++tileCount; m_roadRuns.push_back({{tile.first+0.5f,tile.second+0.5f},tileCount,true});} while(!remainingRoadTiles.empty()){const std::uint64_t key=static_cast<std::uint64_t>(*remainingRoadTiles.begin()); const int tileX=static_cast<std::int32_t>(key>>32); const int tileZ=static_cast<std::int32_t>(static_cast<std::uint32_t>(key)); remainingRoadTiles.erase(static_cast<std::int64_t>(key)); int tileCount=1; while(remainingRoadTiles.erase(roadKey(tileX,tileZ+tileCount))>0) ++tileCount; m_roadRuns.push_back({{tileX+0.5f,tileZ+0.5f},tileCount,false});}}
void Village::ApplyNpcAssets(VillageNpc& npc){const std::size_t index=static_cast<std::size_t>(npc.role); if(index>=m_npcModels.size()) return; npc.model=m_npcModels[index]; npc.animations=m_npcAnimations[index]; npc.animationCount=m_npcAnimationCounts[index];}
void Village::SetModel(const std::string& asset, Model* model){m_models[asset]=model;}
void Village::SetNpcAssets(int index, Model* model, ModelAnimation* animations, int animationCount){if(index<0||index>=static_cast<int>(m_npcModels.size())) return; m_npcModels[static_cast<std::size_t>(index)]=model; m_npcAnimations[static_cast<std::size_t>(index)]=animations; m_npcAnimationCounts[static_cast<std::size_t>(index)]=animationCount; for(VillageNpc& npc:m_npcs) ApplyNpcAssets(npc);}
void Village::HighlightBuilding(int index){if(index<0||index>=static_cast<int>(m_buildings.size())) return; m_highlightedBuilding=index; m_highlightTimer=0.55f;}
void Village::Update(const World& world, NavigationSystem& navigation, Vector3 playerPosition, float deltaTime, float timeOfDay){m_highlightTimer=std::max(0.0f,m_highlightTimer-deltaTime); if(m_highlightTimer==0.0f) m_highlightedBuilding=-1; const bool night=timeOfDay<0.18f||timeOfDay>0.82f; const float simulationDistance=(world.GetRenderRadius()+2.0f)*world.GetConfig().chunkSize*world.GetConfig().tileSize; const float simulationDistanceSquared=simulationDistance*simulationDistance; for(VillageNpc& npc:m_npcs){const float playerDx=npc.position.x-playerPosition.x; const float playerDz=npc.position.z-playerPosition.z; if(playerDx*playerDx+playerDz*playerDz>simulationDistanceSquared) continue; const Vector3 homeDoor=npc.homeBuilding>=0&&npc.homeBuilding<static_cast<int>(m_buildings.size())?DoorPosition(m_buildings[static_cast<std::size_t>(npc.homeBuilding)]):m_center; npc.position.y=world.GetHeightAt(npc.position.x,npc.position.z); const bool daytimeIndoor=npc.role==VillageNpcRole::Mayor||npc.role==VillageNpcRole::Cleric||(npc.role==VillageNpcRole::Innkeeper&&timeOfDay>=0.48f&&timeOfDay<0.66f); const bool wantsHome=night||daytimeIndoor; const auto setDestination=[&](Vector3 destination){std::vector<Vector3> othersPos; othersPos.reserve(m_npcs.size()); for(const VillageNpc& other:m_npcs){if(&other!=&npc) othersPos.push_back(other.position);} navigation.SetOccupiedTiles(othersPos); const std::vector<TileCoord> path=navigation.FindPath(static_cast<int>(std::floor(npc.position.x)),static_cast<int>(std::floor(npc.position.z)),static_cast<int>(std::floor(destination.x)),static_cast<int>(std::floor(destination.z))); if(path.empty()) return false; npc.target=destination; npc.waypoints.clear(); npc.waypoints.reserve(path.size()); for(const TileCoord& tile:path){const float x=static_cast<float>(tile.x)+0.5f; const float z=static_cast<float>(tile.z)+0.5f; npc.waypoints.push_back({x,world.GetHeightAt(x,z),z});} npc.waypointIndex=path.size()>1?1:0; return true;}; if(wantsHome&&!npc.insideHome&&!npc.returningHome){npc.returningHome=setDestination(homeDoor); if(!npc.returningHome) npc.waitTime=0.8f;} else if(!wantsHome&&npc.insideHome){npc.insideHome=false; npc.position=homeDoor; npc.target=homeDoor; npc.waypoints.clear(); npc.waypointIndex=0; npc.returningHome=false; npc.waitTime=0.4f;} if(npc.role==VillageNpcRole::Blacksmith){npc.walking=false;} else if(npc.insideHome){npc.walking=false;} else{if(npc.waypointIndex<npc.waypoints.size()){const Vector3 waypoint=npc.waypoints[npc.waypointIndex]; const float dx=waypoint.x-npc.position.x; const float dz=waypoint.z-npc.position.z; const float distance=std::sqrt(dx*dx+dz*dz); const float step=1.25f*deltaTime; if(distance<=std::max(step,0.04f)){npc.position=waypoint; ++npc.waypointIndex; npc.walking=false; if(npc.waypointIndex>=npc.waypoints.size()){npc.waypoints.clear(); npc.waypointIndex=0; if(npc.returningHome){npc.insideHome=true; npc.returningHome=false;} else{std::uniform_real_distribution<float> waitDist(1.5f,4.0f); npc.waitTime=waitDist(NpcRng());}}} else{npc.position.x+=dx/distance*step; npc.position.z+=dz/distance*step; npc.position.y=world.GetHeightAt(npc.position.x,npc.position.z); npc.facing=std::atan2(dx,dz)*RAD2DEG; npc.walking=true;}} else{npc.walking=false; npc.waitTime=std::max(0.0f,npc.waitTime-deltaTime); if(!wantsHome&&npc.waitTime<=0.0f&&npc.wanderRadius>0.5f){std::uniform_real_distribution<float> angleDist(0.0f,2.0f*3.14159265f); std::uniform_real_distribution<float> radiusDist(1.0f,npc.wanderRadius); bool found=false; for(int attempt=0;attempt<8;++attempt){const float angle=angleDist(NpcRng()); const float r=radiusDist(NpcRng()); const Vector3 dest{npc.spawnPos.x+std::cos(angle)*r,0.0f,npc.spawnPos.z+std::sin(angle)*r}; if(setDestination(dest)){found=true; break;}} if(!found) npc.waitTime=1.5f;} else if(wantsHome&&!npc.returningHome){npc.returningHome=setDestination(homeDoor);}}} if(npc.model&&npc.animations&&npc.animationCount>=1){const bool isWorking=!night&&(npc.role==VillageNpcRole::Blacksmith||npc.role==VillageNpcRole::Farmer); int clip=0; if(npc.walking&&npc.animationCount>=2){clip=1;} clip=std::clamp(clip,0,npc.animationCount-1); if(npc.appliedAnimation!=clip) npc.animationTime=0.0f; npc.animationTime+=deltaTime; const float duration=isWorking?1.0f:(clip==1?1.0f:2.4f); const ModelAnimation& animation=npc.animations[clip]; int frame=static_cast<int>(std::fmod(npc.animationTime,duration)/duration*animation.frameCount); frame=std::clamp(frame,0,animation.frameCount-1); if(npc.appliedAnimation!=clip||npc.appliedFrame!=frame){UpdateModelAnimation(*npc.model,animation,frame); npc.appliedAnimation=clip; npc.appliedFrame=frame;}} if(npc.walking) npc.waitTime=0.0f;}}

Vector3 Village::FindNearestVillage(const World& world, Vector3 position) {
    return FindNearestVillage(world, position, {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()});
}
Vector3 Village::FindNearestVillage(const World& world, Vector3 position, Vector3 excludedVillage) {
    constexpr float regionSize = 192.0f;
    int centerRx = static_cast<int>(std::floor(position.x / regionSize));
    int centerRz = static_cast<int>(std::floor(position.z / regionSize));
    struct Cache { unsigned int seed=0; int rx=0, rz=0; std::vector<Vector3> candidates; bool valid=false; };
    static Cache cache;
    if (!cache.valid || cache.seed!= world.GetSeed() || cache.rx!= centerRx || cache.rz!= centerRz) {
        cache = { world.GetSeed(), centerRx, centerRz, {}, true };
        for (int rx = centerRx - 3; rx <= centerRx + 3; ++rx) {
            for (int rz = centerRz - 3; rz <= centerRz + 3; ++rz) {
                if (auto cand = FindVillageSiteForRegion(world, rx, rz)) cache.candidates.push_back(*cand);
            }
        }
    }
    Vector3 nearest{}; float minDist = std::numeric_limits<float>::max(); bool found=false;
    bool hasExcluded = std::isfinite(excludedVillage.x) && std::isfinite(excludedVillage.z);
    for (auto cand : cache.candidates) {
        if (hasExcluded) { float ex=cand.x-excludedVillage.x; float ez=cand.z-excludedVillage.z; if(ex*ex+ez*ez < 25.0f*25.0f) continue; }
        float dx=cand.x-position.x; float dz=cand.z-position.z; float d=dx*dx+dz*dz;
        if (d < minDist) { minDist=d; nearest=cand; found=true; }
    }
    if (found) return nearest;
    for (int radius=4; radius<=8 &&!found; ++radius) {
        for (int rx=centerRx-radius; rx<=centerRx+radius; ++rx) for (int rz=centerRz-radius; rz<=centerRz+radius; ++rz) {
            if (std::abs(rx-centerRx)<radius && std::abs(rz-centerRz)<radius) continue;
            if (auto cand = FindVillageSiteForRegion(world, rx, rz)) {
                if (hasExcluded){ float ex=cand->x-excludedVillage.x; float ez=cand->z-excludedVillage.z; if(ex*ex+ez*ez < 25.0f*25.0f) continue; }
                return *cand;
            }
        }
    }
    return FindSpawnPosition(world);
}
bool Village::EnsureVillagesNear(const World& world, Vector3 position) {
    constexpr float regionSize = 192.0f;
    const int centerRegionX = static_cast<int>(std::floor(position.x / regionSize));
    const int centerRegionZ = static_cast<int>(std::floor(position.z / regionSize));
    bool addedVillage = false;
    for (int regionX = centerRegionX - 2; regionX <= centerRegionX + 2; ++regionX) {
        for (int regionZ = centerRegionZ - 2; regionZ <= centerRegionZ + 2; ++regionZ) {
            const auto site = FindVillageSiteForRegion(world, regionX, regionZ);
            if (!site) continue;
            bool overlapsExisting = false;
            for (const Vector3 existing : m_villageCenters) {
                float cdx = site->x - existing.x; float cdz = site->z - existing.z;
                if (cdx*cdx + cdz*cdz < 70.0f*70.0f) { overlapsExisting = true; break; }
            }
            if (overlapsExisting) continue;
            const uint32_t yawSeed = world.GetSeed() ^ (uint32_t)regionX*73856093u ^ (uint32_t)regionZ*19349663u;
            float yaw = (float)((yawSeed>>2)%4u)*90.0f;
            if (AddVillageLayout(*site, yaw, yawSeed>>2)) {
                TraceLog(LOG_INFO, "[VILLAGE] Generated at %.1f %.1f", site->x, site->z);
                addedVillage = true;
            }
        }
    }
    if (addedVillage) RebuildRoadRuns();
    return addedVillage;
}
bool Village::PruneDistantVillages(Vector3 position, float maxDistance) {
    const float maxDistanceSquared = maxDistance * maxDistance;
    const auto keepCenter = [&](Vector3 center) {
        if (std::fabs(center.x - m_center.x) < 0.5f && std::fabs(center.z - m_center.z) < 0.5f) return true;
        const float dx = center.x - position.x; const float dz = center.z - position.z;
        return dx*dx+dz*dz <= maxDistanceSquared;
    };
    std::vector<Vector3> keptCenters; for(const Vector3 center:m_villageCenters){if(keepCenter(center)) keptCenters.push_back(center);} if(keptCenters.size()==m_villageCenters.size()) return false;
    std::vector<int> buildingRemap(m_buildings.size(),-1); std::vector<Building> keptBuildings; keptBuildings.reserve(m_buildings.size());
    for(std::size_t oldIndex=0;oldIndex<m_buildings.size();++oldIndex){if(!keepCenter(m_buildings[oldIndex].center)) continue; buildingRemap[oldIndex]=static_cast<int>(keptBuildings.size()); keptBuildings.push_back(std::move(m_buildings[oldIndex]));}
    std::vector<Instance> keptInstances; keptInstances.reserve(m_instances.size()); for(Instance& instance:m_instances){if(!keepCenter(instance.center)) continue; if(instance.buildingIndex>=0){instance.buildingIndex=buildingRemap[static_cast<std::size_t>(instance.buildingIndex)]; if(instance.buildingIndex<0) continue;} keptInstances.push_back(std::move(instance));}
    std::vector<VillageNpc> keptNpcs; keptNpcs.reserve(m_npcs.size()); for(VillageNpc& npc:m_npcs){if(!keepCenter(npc.villageCenter)||npc.homeBuilding<0||buildingRemap[static_cast<std::size_t>(npc.homeBuilding)]<0) continue; npc.homeBuilding=buildingRemap[static_cast<std::size_t>(npc.homeBuilding)]; keptNpcs.push_back(std::move(npc));}
    std::vector<InteriorChest> keptChests; keptChests.reserve(m_chests.size()); for(InteriorChest& chest:m_chests){if(chest.buildingIndex<0||buildingRemap[static_cast<std::size_t>(chest.buildingIndex)]<0) continue; chest.buildingIndex=buildingRemap[static_cast<std::size_t>(chest.buildingIndex)]; keptChests.push_back(std::move(chest));}
    if(m_highlightedBuilding>=0&&m_highlightedBuilding<static_cast<int>(buildingRemap.size())){m_highlightedBuilding=buildingRemap[static_cast<std::size_t>(m_highlightedBuilding)];}
    m_buildings=std::move(keptBuildings); m_instances=std::move(keptInstances); m_npcs=std::move(keptNpcs); m_chests=std::move(keptChests); m_villageCenters=std::move(keptCenters); RebuildRoadRuns(); return true;
}
Vector3 Village::FindSpawnPosition(const World& world) {
    for (int radius=0; radius<=160; ++radius) {
        for (int x=-radius; x<=radius; ++x) for (int side : {-1,1}) { const int z=side*radius; if((x&3)||(z&3)) continue; const float centerX=x+0.5f; const float centerZ=z+0.5f; if(IsVillageSite(world,centerX,centerZ)) return {centerX, world.GetHeightAt(centerX,centerZ), centerZ};}
        for (int z=-radius+1; z<radius; ++z) for (int side : {-1,1}) { const int x=side*radius; if((x&3)||(z&3)) continue; const float centerX=x+0.5f; const float centerZ=z+0.5f; if(IsVillageSite(world,centerX,centerZ)) return {centerX, world.GetHeightAt(centerX,centerZ), centerZ};}
    }
    for (int radius=0; radius<=64; ++radius) for (int x=-radius; x<=radius; ++x) for (int z : {-radius, radius}) { const float centerX=x+0.5f; const float centerZ=z+0.5f; if(!world.IsWaterAt(centerX,centerZ)) return {centerX, world.GetHeightAt(centerX,centerZ), centerZ};}
    return {0.5f, world.GetHeightAt(0.5f,0.5f), 0.5f};
}
std::vector<Rectangle> Village::CollisionBounds() const {
    std::vector<Rectangle> bounds; bounds.reserve(m_buildings.size()*4+m_instances.size());
    const auto addLocalObstacle=[&](const Building& building,float localX,float localZ,float width,float depth){const Vector2 offset=RotateLocal(localX,localZ,building.yawDegrees); const float yaw=building.yawDegrees*DEG2RAD; const float halfX=std::fabs(std::cos(yaw))*width*0.5f+std::fabs(std::sin(yaw))*depth*0.5f; const float halfZ=std::fabs(std::sin(yaw))*width*0.5f+std::fabs(std::cos(yaw))*depth*0.5f; const float worldX=building.center.x+building.position.x+offset.x; const float worldZ=building.center.z+building.position.z+offset.y; bounds.push_back({worldX-halfX,worldZ-halfZ,halfX*2.0f,halfZ*2.0f});};
    constexpr float wall=0.30f; constexpr float doorWidth=1.20f;
    for(const Building& building:m_buildings){addLocalObstacle(building,-building.halfWidth+wall*0.5f,0.0f,wall,building.halfDepth*2.0f); addLocalObstacle(building,building.halfWidth-wall*0.5f,0.0f,wall,building.halfDepth*2.0f); addLocalObstacle(building,0.0f,-building.halfDepth+wall*0.5f,building.halfWidth*2.0f,wall); const float frontSegment=std::max(0.0f,building.halfWidth-doorWidth*0.5f); if(frontSegment>0.0f){const float segmentWidth=frontSegment; const float segmentOffset=doorWidth*0.5f+segmentWidth*0.5f; addLocalObstacle(building,-segmentOffset,building.halfDepth-wall*0.5f,segmentWidth,wall); addLocalObstacle(building,segmentOffset,building.halfDepth-wall*0.5f,segmentWidth,wall);}}
    for(const Instance& instance:m_instances){if(instance.buildingIndex>=0||instance.collisionRadius<0.75f||instance.asset.rfind("village/props/",0)!=0) continue; const Vector2 offset=RotateLocal(instance.position.x,instance.position.z,0.0f); const float radius=instance.collisionRadius*instance.scale; bounds.push_back({instance.center.x+instance.position.x+offset.x-radius,instance.center.z+instance.position.z+offset.y-radius,radius*2.0f,radius*2.0f});}
    return bounds;
}
Vector3 Village::BuildingDoorPosition(int buildingIndex) const {if(buildingIndex<0||buildingIndex>=static_cast<int>(m_buildings.size())) return {}; return DoorPosition(m_buildings[static_cast<std::size_t>(buildingIndex)]);}
const VillageNpc* Village::NpcAtTile(int tileX, int tileZ) const {for(const VillageNpc& npc:m_npcs){if(static_cast<int>(std::floor(npc.position.x))==tileX&&static_cast<int>(std::floor(npc.position.z))==tileZ) return &npc;} return nullptr;}
const VillageNpc* Village::NpcAtRay(const World& world, Ray ray) const {const VillageNpc* closestNpc=nullptr; float closestDistance=100000.0f; for(const VillageNpc& npc:m_npcs){if(npc.insideHome||!IsChunkVisible(world,npc.position,ray.position)) continue; const BoundingBox bounds={{npc.position.x-0.38f,npc.position.y,npc.position.z-0.38f},{npc.position.x+0.38f,npc.position.y+1.9f,npc.position.z+0.38f}}; const RayCollision hit=GetRayCollisionBox(ray,bounds); if(hit.hit&&hit.distance<closestDistance){closestNpc=&npc; closestDistance=hit.distance;}} return closestNpc;}
bool Village::NpcAtInteriorRay(Ray ray, int buildingIndex, VillageNpc& outNpc) const {int interiorNpcIndex=0; float closestDistance=100000.0f; bool found=false; for(const VillageNpc& npc:m_npcs){if(!npc.insideHome||npc.homeBuilding!=buildingIndex) continue; const Vector3 position{-2.6f+(interiorNpcIndex%2)*1.1f,0.0f,-0.5f+(interiorNpcIndex/2)*1.2f}; ++interiorNpcIndex; const BoundingBox bounds={{position.x-0.38f,position.y,position.z-0.38f},{position.x+0.38f,position.y+1.9f,position.z+0.38f}}; const RayCollision hit=GetRayCollisionBox(ray,bounds); if(hit.hit&&hit.distance<closestDistance){closestDistance=hit.distance; outNpc=npc; outNpc.position=position; found=true;}} return found;}
InteriorChest* Village::ChestAtInteriorRay(Ray ray, int buildingIndex){float closestDistance=100000.0f; InteriorChest* foundChest=nullptr; for(auto& chest:m_chests){if(chest.buildingIndex!=buildingIndex) continue; const BoundingBox bounds={{chest.position.x-0.4f,chest.position.y,chest.position.z-0.4f},{chest.position.x+0.4f,chest.position.y+0.6f,chest.position.z+0.4f}}; const RayCollision hit=GetRayCollisionBox(ray,bounds); if(hit.hit&&hit.distance<closestDistance){closestDistance=hit.distance; foundChest=&chest;}} return foundChest;}
int Village::BuildingDoorAtTile(int tileX, int tileZ) const {for(int index=0;index<static_cast<int>(m_buildings.size());++index){const Building& building=m_buildings[static_cast<std::size_t>(index)]; const Vector3 door=DoorPosition(building); const float doorX=door.x; const float doorZ=door.z; if(static_cast<int>(std::floor(doorX))==tileX&&static_cast<int>(std::floor(doorZ))==tileZ) return index;} return -1;}
int Village::BuildingDoorAtRay(const World& world, Ray ray) const {int closestBuilding=-1; float closestDistance=100000.0f; for(int index=0;index<static_cast<int>(m_buildings.size());++index){const Building& building=m_buildings[static_cast<std::size_t>(index)]; const float yaw=building.yawDegrees*DEG2RAD; const float x=building.center.x+building.position.x; const float z=building.center.z+building.position.z; if(!IsChunkVisible(world,{x,0.0f,z},ray.position)) continue; const float ground=world.GetHeightAt(x,z); const Vector2 faceOffset=RotateLocal(building.doorStep.x,building.doorStep.z-0.65f,building.yawDegrees); const Vector3 doorFace{building.center.x+building.position.x+faceOffset.x,ground+1.47f,building.center.z+building.position.z+faceOffset.y}; const float doorHalfX=std::fabs(std::cos(yaw))*0.60f+std::fabs(std::sin(yaw))*0.16f; const float doorHalfZ=std::fabs(std::sin(yaw))*0.60f+std::fabs(std::cos(yaw))*0.16f; const BoundingBox doorBounds={{doorFace.x-doorHalfX,ground+0.42f,doorFace.z-doorHalfZ},{doorFace.x+doorHalfX,ground+2.52f,doorFace.z+doorHalfZ}}; const RayCollision collision=GetRayCollisionBox(ray,doorBounds); if(collision.hit&&collision.distance<closestDistance){closestBuilding=index; closestDistance=collision.distance;}} return closestBuilding;}
std::vector<Vector3> Village::NpcPositions() const {std::vector<Vector3> positions; positions.reserve(m_npcs.size()); for(const VillageNpc& npc:m_npcs){if(!npc.insideHome) positions.push_back(npc.position);} return positions;}
std::vector<Vector3> Village::NpcPositions(Vector3 center, float maxDistance) const {const float maxDistanceSquared=maxDistance*maxDistance; std::vector<Vector3> positions; for(const VillageNpc& npc:m_npcs){if(npc.insideHome) continue; const float dx=npc.position.x-center.x; const float dz=npc.position.z-center.z; if(dx*dx+dz*dz<=maxDistanceSquared) positions.push_back(npc.position);} return positions;}
Vector3 Village::NearestVillageCenter(Vector3 position) const {Vector3 nearest=m_center; float nearestDistance=std::numeric_limits<float>::max(); for(const Vector3 center:m_villageCenters){const float dx=center.x-position.x; const float dz=center.z-position.z; const float distance=dx*dx+dz*dz; if(distance<nearestDistance){nearestDistance=distance; nearest=center;}} return nearest;}
std::string Village::BuildingNameKey(int buildingIndex) const {if(buildingIndex<0||buildingIndex>=static_cast<int>(m_buildings.size())) return "village.building.home"; const std::string& kind=m_buildings[static_cast<std::size_t>(buildingIndex)].kind; if(kind=="forge") return "village.building.forge"; if(kind=="hall") return "village.building.hall"; if(kind=="chapel") return "village.building.chapel"; if(kind=="inn") return "village.building.inn"; if(kind=="farmhouse") return "village.building.farmhouse"; return "village.building.home";}
std::string Village::BuildingKind(int buildingIndex) const {if(buildingIndex<0||buildingIndex>=static_cast<int>(m_buildings.size())) return {}; return m_buildings[static_cast<std::size_t>(buildingIndex)].kind;}
#define DrawCube DrawLitCube
void Village::Render(const World& world, Vector3 playerPosition) const {
    const float roadRenderRange=(world.GetRenderRadius()+1.0f)*world.GetConfig().chunkSize*world.GetConfig().tileSize;
    for(const RoadRun& run:m_roadRuns){const float endX=run.start.x+(run.alongX?run.tileCount-1.0f:0.0f); const float endZ=run.start.y+(run.alongX?0.0f:run.tileCount-1.0f); if(std::max(run.start.x,endX)<playerPosition.x-roadRenderRange||std::min(run.start.x,endX)>playerPosition.x+roadRenderRange||std::max(run.start.y,endZ)<playerPosition.z-roadRenderRange||std::min(run.start.y,endZ)>playerPosition.z+roadRenderRange) continue; int segmentLength=0; float segmentGround=0.0f; const auto flushSegment=[&](){if(segmentLength==0) return; const float offset=(segmentLength-1)*0.5f; const Vector3 position{run.start.x+(run.alongX?offset:0.0f),segmentGround+0.035f,run.start.y+(run.alongX?0.0f:offset)}; const float length=segmentLength*0.98f; DrawCube(position,run.alongX?length:0.98f,0.07f,run.alongX?0.98f:length,Color{128,116,96,255}); segmentLength=0;}; for(int tileIndex=0;tileIndex<=run.tileCount;++tileIndex){const Vector3 tile{run.start.x+(run.alongX?static_cast<float>(tileIndex):0.0f),0.0f,run.start.y+(run.alongX?0.0f:static_cast<float>(tileIndex))}; const bool visible=tileIndex<run.tileCount&&IsChunkVisible(world,tile,playerPosition)&&!world.IsWaterAt(tile.x,tile.z); const float ground=visible?world.GetHeightAt(tile.x,tile.z):0.0f; if(!visible||(segmentLength>0&&std::fabs(ground-segmentGround)>0.001f)) flushSegment(); if(visible){if(segmentLength==0) segmentGround=ground; ++segmentLength;}}}
    for(const Instance& instance:m_instances){const float x=instance.center.x+instance.position.x; const float z=instance.center.z+instance.position.z; const Vector3 worldPosition{x,0.0f,z}; if(!IsChunkVisible(world,worldPosition,playerPosition)||world.IsWaterAt(x,z)) continue; const auto modelEntry=m_models.find(instance.asset); if(modelEntry==m_models.end()||!modelEntry->second||modelEntry->second->meshCount==0) continue; float ground=world.GetHeightAt(x,z); if(instance.buildingIndex>=0){ground=world.GetHeightAt(x,z);} const Color tint=instance.buildingIndex==m_highlightedBuilding&&m_highlightTimer>0.0f?Color{255,218,102,255}:WHITE; DrawModelEx(*modelEntry->second,{x,ground+instance.position.y,z},{0.0f,1.0f,0.0f},instance.yawDegrees,{instance.scale,instance.scale,instance.scale},tint);}
    for(const VillageNpc& npc:m_npcs){if(!npc.insideHome&&IsChunkVisible(world,npc.position,playerPosition)){Vector3 position=npc.position; position.y=world.GetHeightAt(position.x,position.z)+0.04f; DrawResident(npc,position);}}
}
void Village::RenderInterior(int buildingIndex, Vector3 cameraPosition, Vector3 playerPosition, Ray hoverRay) const {
    const std::string buildingKind=buildingIndex>=0&&buildingIndex<static_cast<int>(m_buildings.size())?m_buildings[static_cast<std::size_t>(buildingIndex)].kind:std::string{};
    const Color timber{83,58,39,255}; const Color plaster{142,115,82,255}; const Color floor{105,80,58,255};
    const Vector3 viewVector=Vector3Subtract(playerPosition,cameraPosition); const float viewDistance=Vector3Length(viewVector); const Ray viewRay={cameraPosition,viewDistance>0.001f?Vector3Scale(viewVector,1.0f/viewDistance):Vector3{0.0f,0.0f,0.0f}};
    const auto drawInteriorCube=[&](Vector3 position,float width,float height,float depth,Color color){const BoundingBox bounds={{position.x-width*0.5f,position.y-height*0.5f,position.z-depth*0.5f},{position.x+width*0.5f,position.y+height*0.5f,position.z+depth*0.5f}}; bool occludesPlayer=false; if(viewDistance>0.001f){if(bounds.min.z>playerPosition.z-0.2f&&bounds.min.y<playerPosition.y+2.0f&&bounds.max.y>playerPosition.y){if(bounds.max.x>playerPosition.x-1.0f&&bounds.min.x<playerPosition.x+1.0f){occludesPlayer=true;}} if(!occludesPlayer){const RayCollision hit=GetRayCollisionBox(viewRay,bounds); occludesPlayer=hit.hit&&hit.distance<viewDistance-0.18f;}} if(occludesPlayer){BeginBlendMode(BLEND_ALPHA); rlDisableDepthMask(); DrawCube(position,width,height,depth,Fade(color,0.18f)); rlEnableDepthMask(); EndBlendMode();} else{DrawCube(position,width,height,depth,color);}};
    drawInteriorCube({0.0f,-0.18f,0.0f},10.0f,0.36f,9.0f,floor); drawInteriorCube({0.0f,1.5f,-4.35f},10.0f,3.0f,0.3f,plaster); drawInteriorCube({-4.85f,1.5f,0.0f},0.3f,3.0f,9.0f,plaster); drawInteriorCube({4.85f,1.5f,0.0f},0.3f,3.0f,9.0f,plaster); drawInteriorCube({-2.65f,1.5f,4.35f},4.7f,3.0f,0.3f,plaster); drawInteriorCube({2.65f,1.5f,4.35f},4.7f,3.0f,0.3f,plaster); drawInteriorCube({0.0f,2.95f,4.35f},1.0f,0.2f,0.3f,timber);
    constexpr float gridCellSize=1.0f; for(int cellX=0;cellX<10;++cellX){for(int cellZ=0;cellZ<9;++cellZ){const float x=-4.5f+static_cast<float>(cellX); const float z=-4.0f+static_cast<float>(cellZ); DrawCubeWires({x,0.005f,z},gridCellSize*0.96f,0.005f,gridCellSize*0.96f,Color{75,55,35,140});}}
    if(std::fabs(hoverRay.direction.y)>0.0001f){const float floorDistance=-hoverRay.position.y/hoverRay.direction.y; if(floorDistance>0.0f){const float hitX=hoverRay.position.x+hoverRay.direction.x*floorDistance; const float hitZ=hoverRay.position.z+hoverRay.direction.z*floorDistance; const int cellX=static_cast<int>(std::floor(hitX+5.0f)); const int cellZ=static_cast<int>(std::floor(hitZ+4.5f)); if(cellX>=0&&cellX<10&&cellZ>=0&&cellZ<9){DrawCube({-4.5f+cellX,0.025f,-4.0f+cellZ},0.92f,0.025f,0.92f,Color{224,184,92,115});}}}
    if(buildingKind=="forge"){drawInteriorCube({-3.0f,0.55f,-2.0f},2.0f,1.1f,1.3f,Color{65,57,50,255}); drawInteriorCube({-3.0f,1.2f,-2.0f},1.0f,0.35f,0.6f,ORANGE); drawInteriorCube({1.0f,0.52f,0.0f},2.4f,1.0f,1.0f,timber); drawInteriorCube({1.0f,1.08f,0.0f},0.25f,0.12f,0.25f,LIGHTGRAY);} else if(buildingKind=="hall"){drawInteriorCube({0.0f,0.62f,-1.6f},3.8f,1.2f,1.25f,timber); drawInteriorCube({-3.4f,1.45f,-2.7f},0.35f,2.5f,1.3f,DARKBROWN); drawInteriorCube({-3.4f,1.9f,-1.7f},0.35f,0.08f,1.3f,GOLD); drawInteriorCube({3.4f,1.45f,-2.7f},0.35f,2.5f,1.3f,DARKBROWN);} else if(buildingKind=="chapel"){drawInteriorCube({0.0f,0.42f,-2.7f},2.6f,0.8f,1.0f,Color{174,156,119,255}); drawInteriorCube({0.0f,1.2f,-2.7f},0.12f,1.0f,0.12f,GOLD); drawInteriorCube({0.0f,1.45f,-2.7f},0.5f,0.12f,0.12f,GOLD); DrawCandle({-1.0f,0.85f,-2.7f},0.12f,0.35f,8,Color{240,174,65,255}); DrawCandle({1.0f,0.85f,-2.7f},0.12f,0.35f,8,Color{240,174,65,255});} else{drawInteriorCube({0.0f,0.65f,-3.0f},6.6f,1.3f,0.8f,Color{105,67,42,255}); drawInteriorCube({-2.7f,0.35f,0.0f},1.5f,0.7f,1.3f,timber); drawInteriorCube({2.7f,0.35f,0.0f},1.5f,0.7f,1.3f,timber); drawInteriorCube({0.0f,0.34f,1.7f},2.0f,0.68f,1.1f,timber);}
    int interiorNpcIndex=0; for(const VillageNpc& npc:m_npcs){if(!npc.insideHome||npc.homeBuilding!=buildingIndex) continue; const Vector3 position{-2.6f+(interiorNpcIndex%2)*1.1f,0.0f,-0.5f+(interiorNpcIndex/2)*1.2f}; DrawResident(npc,position); ++interiorNpcIndex;}
    for(const auto& chest:m_chests){if(chest.buildingIndex!=buildingIndex) continue; Color chestColor=chest.looted?Color{100,80,60,255}:Color{180,120,40,255}; drawInteriorCube({chest.position.x,chest.position.y+0.3f,chest.position.z},0.8f,0.6f,0.6f,chestColor); drawInteriorCube({chest.position.x,chest.position.y+0.3f,chest.position.z+0.31f},0.2f,0.2f,0.05f,GOLD);}
}