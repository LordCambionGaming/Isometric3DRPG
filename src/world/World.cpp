#include "world/World.h"
#include "world/Chunk.h"
#include "world/TerrainGenerator.h"
#include <iostream>
#include <cmath>
#include <chrono>
#include <algorithm>
#include <rlgl.h>

World::World()
    : m_terrainGenerator(nullptr)
    , m_lastPlayerPosition({0.0f, 0.0f, 0.0f})
    , m_lastChunkX(0)
    , m_lastChunkZ(0)
    , m_lastUpdateTime(0.0f)
    , m_initialized(false)
{
}

World::~World() {
}
void World::SetGlobalShader(Shader shader) {
    m_globalShader = shader;
    m_hasGlobalShader = true;
    for (auto& pair : m_chunks) {
        if (pair.second) pair.second->SetShader(shader);
    }

}

bool World::Initialize(const WorldConfig& config) { /* tuo codice identico */
    try {
        m_config = config;
        m_terrainGenerator = std::make_unique<TerrainGenerator>();
        m_terrainGenerator->Initialize(m_config.seed);
        m_lastUpdateTime = 0.0f;
        m_chunkLoadTimer = 0.0f;
        m_biomeManager.Initialize();
        m_initialized = true;
        TraceLog(LOG_INFO, "World initialized with seed: %u", m_config.seed);
        return true;
    } catch (const std::exception& e) {
        TraceLog(LOG_ERROR, "Exception in World::Initialize: %s", e.what());
        return false;
    }
}

void World::SetPlayerPosition(Vector3 pos) { m_lastPlayerPosition = pos; }
void World::Update(float deltaTime) { /* tuo codice identico */
    if (!m_initialized) return;
    Vector3 currentPosition = m_lastPlayerPosition;
    int cx, cz; WorldToChunk(currentPosition.x, currentPosition.z, cx, cz);
    bool chunkChanged = (cx!= m_lastChunkX || cz!= m_lastChunkZ);
    if (chunkChanged || m_lastUpdateTime >= CHUNK_UPDATE_INTERVAL) {
        if (chunkChanged) { m_lastChunkX = cx; m_lastChunkZ = cz; }
        UpdateChunkStreaming(currentPosition);
        m_lastUpdateTime = 0.0f;
    } else { m_lastUpdateTime += deltaTime; }
    if (pendingChunks.empty()) { m_chunkLoadTimer = 0.0f; } else {
        m_chunkLoadTimer += deltaTime;
        if (m_chunkLoadTimer >= CHUNK_LOAD_INTERVAL) { ProcessPendingChunks(); m_chunkLoadTimer = 0.0f; }
    }
}
void World::Render(bool withWater) {
    if (!m_initialized) return;

    // Pass 1: tutti gli opachi (terra + fondale sabbia)
    for (const auto& pair : m_chunks) {
        const auto& chunk = pair.second;
        if (chunk && chunk->IsGenerated() && chunk->HasMesh()) chunk->Render();
    }

    // Pass 2: acqua trasparente. Depth write off, così l'acqua non copre
    // la terra disegnata dopo (in caso di chunk lontani) e non nasconde le
    // altre acque dietro di sé. Depth test resta on, così l'acqua è occlusa
    // correttamente da qualunque geometria opaca più vicina alla camera.
    if (withWater) {
        rlDisableDepthMask();
        for (const auto& pair : m_chunks) {
            const auto& chunk = pair.second;
            if (chunk && chunk->IsGenerated() && chunk->HasWaterMesh()) chunk->RenderWater();
        }
        rlEnableDepthMask();
    }
}
void World::SetSeed(unsigned int seed) { if(m_config.seed==seed) return; m_config.seed=seed; if(m_terrainGenerator) m_terrainGenerator->SetSeed(seed); m_chunks.clear(); GenerateInitialChunks(); }
Chunk* World::GetChunk(int x,int z){ ChunkKey key={x,z}; auto it=m_chunks.find(key); return (it!=m_chunks.end())?it->second.get():nullptr; }

void World::LoadChunkAt(int x, int z) {
    ChunkKey key = {x, z};
    if (m_chunks.find(key) != m_chunks.end()) return;

    auto chunk = std::make_unique<Chunk>(x, z, m_config.chunkSize, m_config.tileSize);
    if (m_hasGlobalShader) chunk->SetShader(m_globalShader);
    chunk->Generate(*m_terrainGenerator, this);

    // Applica flatten/asciugatura subito dopo la generazione: cosi' ogni chunk (anche quelli
    // caricati in streaming mentre il player si muove) nasce gia' piatto e asciutto dentro le
    // flatten area registrate da ForceDryArea, senza dover ri-chiamare ForceDryArea ogni volta.
    if (!m_flattenAreas.empty()) {
        Vector3 wp = chunk->GetWorldPosition();
        for (int zz = 0; zz < m_config.chunkSize; ++zz) {
            for (int xx = 0; xx < m_config.chunkSize; ++xx) {
                float wx = wp.x + xx * m_config.tileSize + 0.5f;
                float wz = wp.z + zz * m_config.tileSize + 0.5f;
                float h;
                if (GetFlattenedHeight(wx, wz, h)) {
                    TerrainTile& tile = chunk->GetTile(xx, zz);
                    tile.height      = h;
                    tile.hasWater    = false;
                    tile.waterHeight = 0.0f;
                }
            }
        }
    }

    chunk->BuildMesh();
    m_chunks[key] = std::move(chunk);
}
void World::ProcessPendingChunks() {
    if (pendingChunks.empty()) return;
    ChunkKey k = pendingChunks.front();
    pendingChunks.erase(pendingChunks.begin());
    if (IsChunkLoaded(k.x, k.z)) return;

    auto chunk = std::make_unique<Chunk>(k.x, k.z, m_config.chunkSize, m_config.tileSize);
    if (m_hasGlobalShader) chunk->SetShader(m_globalShader);
    chunk->Generate(*m_terrainGenerator, this);

    if (!m_flattenAreas.empty()) {
        Vector3 wp = chunk->GetWorldPosition();
        for (int zz = 0; zz < m_config.chunkSize; ++zz) {
            for (int xx = 0; xx < m_config.chunkSize; ++xx) {
                float wx = wp.x + xx * m_config.tileSize + 0.5f;
                float wz = wp.z + zz * m_config.tileSize + 0.5f;
                float h;
                if (GetFlattenedHeight(wx, wz, h)) {
                    TerrainTile& tile = chunk->GetTile(xx, zz);
                    tile.height      = h;
                    tile.hasWater    = false;
                    tile.waterHeight = 0.0f;
                }
            }
        }
    }

    chunk->BuildMesh();
    m_chunks[{k.x, k.z}] = std::move(chunk);
}
void World::UnloadChunk(int x,int z){ ChunkKey key={x,z}; auto it=m_chunks.find(key); if(it!=m_chunks.end()){ it->second->Unload(); m_chunks.erase(it);} }
void World::WorldToChunk(float worldX,float worldZ,int& chunkX,int& chunkZ) const { float chunkWorldSize=m_config.chunkSize*m_config.tileSize; chunkX=static_cast<int>(std::floor(worldX/chunkWorldSize)); chunkZ=static_cast<int>(std::floor(worldZ/chunkWorldSize)); }
void World::WorldToChunk(float worldX,float worldZ,int& chunkX,int& chunkZ,int& localX,int& localZ) const { float chunkWorldSize=m_config.chunkSize*m_config.tileSize; chunkX=static_cast<int>(std::floor(worldX/chunkWorldSize)); chunkZ=static_cast<int>(std::floor(worldZ/chunkWorldSize)); float localWorldX=worldX-(chunkX*chunkWorldSize); float localWorldZ=worldZ-(chunkZ*chunkWorldSize); localX=static_cast<int>(std::floor(localWorldX/m_config.tileSize)); localZ=static_cast<int>(std::floor(localWorldZ/m_config.tileSize)); localX=std::max(0,std::min(localX,m_config.chunkSize-1)); localZ=std::max(0,std::min(localZ,m_config.chunkSize-1)); }
Vector3 World::ChunkToWorld(int chunkX,int chunkZ) const { float chunkWorldSize=m_config.chunkSize*m_config.tileSize; return {chunkX*chunkWorldSize,0.0f,chunkZ*chunkWorldSize}; }
Vector3 World::ChunkToWorldCenter(int chunkX,int chunkZ) const { float chunkWorldSize=m_config.chunkSize*m_config.tileSize; return {chunkX*chunkWorldSize+chunkWorldSize*0.5f,0.0f,chunkZ*chunkWorldSize+chunkWorldSize*0.5f}; }

bool World::GetFlattenedHeight(float wx, float wz, float& outHeight) const {
    for (auto& a : m_flattenAreas) {
        float dx = wx - a.x; float dz = wz - a.z;
        float dist = sqrtf(dx*dx + dz*dz);
        
        float angle = atan2f(dz, dx);
        float n = VillageHashAngle(a.x, a.z, angle);
        float irregularRadius = a.radius + (n * 14.0f - 7.0f); // + -7 variazione
        
        float innerRadius = a.radius - 8.0f; // 0..(radius-8) sempre piatto, garantito

        if (dist <= innerRadius) {
            outHeight = a.height;
            return true;
        }
        if (dist <= irregularRadius) {
            // sei nella fascia di bordo rumorosa, ma ancora piatto
            outHeight = a.height;
            return true;
        }
    }
    return false;
}

void World::ForceDryArea(float centerX, float centerZ, float radius) {
    float villageHeight = 6.0f;
    bool exists = false;
    for (auto& fa : m_flattenAreas) {
        float dx = fa.x - centerX; float dz = fa.z - centerZ;
        if (dx*dx + dz*dz < 100.0f) { exists = true; break; }
    }
    if (!exists) m_flattenAreas.push_back({centerX, centerZ, radius, villageHeight});

    for (auto& pair : m_chunks) {
        Chunk* chunk = pair.second.get();
        if (!chunk || !chunk->IsGenerated()) continue;
        Vector3 wp = chunk->GetWorldPosition();
        bool dirty = false;
        for (int z = 0; z < m_config.chunkSize; ++z) {
            for (int x = 0; x < m_config.chunkSize; ++x) {
                float wx = wp.x + x * m_config.tileSize + 0.5f;
                float wz = wp.z + z * m_config.tileSize + 0.5f;
                float dx = wx - centerX; float dz = wz - centerZ;
                float dist = sqrtf(dx*dx + dz*dz);
                float angle = atan2f(dz, dx);
                float n = VillageHashAngle(centerX, centerZ, angle);
                float irregularRadius = radius + (n * 14.0f - 7.0f);
                float innerRadius = radius - 8.0f;

                if (dist <= irregularRadius) {
                    TerrainTile& tile = chunk->GetTile(x,z);
                    tile.height = villageHeight;
                    tile.hasWater = false;
                    tile.waterHeight = 0.0f;
                    dirty = true;
                }
            }
        }
        if (dirty) chunk->BuildMesh();
    }
}

void World::UpdateChunkStreaming(Vector3 centerPosition){
    int centerChunkX, centerChunkZ; WorldToChunk(centerPosition.x, centerPosition.z, centerChunkX, centerChunkZ);
    std::vector<ChunkKey> needed;
    for(int x=centerChunkX-GENERATION_RADIUS;x<=centerChunkX+GENERATION_RADIUS;x++) for(int z=centerChunkZ-GENERATION_RADIUS;z<=centerChunkZ+GENERATION_RADIUS;z++) if(GetChunkDistanceSquared(x,z,centerPosition)<=GENERATION_RADIUS*GENERATION_RADIUS) needed.push_back({x,z});
    std::vector<ChunkKey> cleaned; for(const ChunkKey& k:pendingChunks){ bool inChunks=IsChunkLoaded(k.x,k.z); bool stillNeeded=false; for(const ChunkKey& need:needed) if(need.x==k.x&&need.z==k.z){stillNeeded=true;break;} if(!inChunks&&stillNeeded) cleaned.push_back(k); } pendingChunks=cleaned;
    for(const ChunkKey& k:needed){ bool alreadyInChunks=IsChunkLoaded(k.x,k.z); bool alreadyPending=false; for(const ChunkKey& p:pendingChunks) if(p.x==k.x&&p.z==k.z){alreadyPending=true;break;} if(!alreadyInChunks&&!alreadyPending) pendingChunks.push_back(k); }
    std::sort(pendingChunks.begin(),pendingChunks.end(),[centerChunkX,centerChunkZ](const ChunkKey& a,const ChunkKey& b){ int da=(a.x-centerChunkX)*(a.x-centerChunkX)+(a.z-centerChunkZ)*(a.z-centerChunkZ); int db=(b.x-centerChunkX)*(b.x-centerChunkX)+(b.z-centerChunkZ)*(b.z-centerChunkZ); if(da!=db) return da<db; if(a.x!=b.x) return a.x<b.x; return a.z<b.z; });
    UnloadDistantChunks(centerPosition); m_lastPlayerPosition=centerPosition;
}
void World::UnloadDistantChunks(Vector3 centerPosition){ int centerCX,centerCZ; WorldToChunk(centerPosition.x,centerPosition.z,centerCX,centerCZ); std::vector<ChunkKey> chunksToUnload; for(const auto& pair:m_chunks){ const ChunkKey& key=pair.first; int dx=key.x-centerCX; int dz=key.z-centerCZ; float dSq=(float)(dx*dx+dz*dz); if(dSq>UNLOAD_RADIUS*UNLOAD_RADIUS) chunksToUnload.push_back(key);} for(const ChunkKey& key:chunksToUnload) UnloadChunk(key.x,key.z); }
bool World::IsChunkLoaded(int chunkX,int chunkZ) const { ChunkKey key={chunkX,chunkZ}; return m_chunks.find(key)!=m_chunks.end(); }

float World::GetHeightAt(float worldX, float worldZ) const {
    if (!m_terrainGenerator) return 0.0f;
    int chunkX, chunkZ, localX, localZ;
    WorldToChunk(worldX, worldZ, chunkX, chunkZ, localX, localZ);
    Chunk* chunk = const_cast<World*>(this)->GetChunk(chunkX, chunkZ);
    if (chunk && chunk->IsGenerated()) {
        const TerrainTile& tile = chunk->GetTile(localX, localZ);
        return tile.hasWater ? tile.waterHeight : tile.height;  // niente più 2.0f
    }
     int worldXInt = (int)std::floor(worldX / m_config.tileSize);
    int worldZInt = (int)std::floor(worldZ / m_config.tileSize);
    int level = m_terrainGenerator->GenerateHeightLevel(worldXInt, worldZInt);
    if (level < TerrainGenerator::SEA_LEVEL) return (float)TerrainGenerator::SEA_LEVEL;
    return (float)level;
}
int World::GetBiomeAt(float worldX,float worldZ) const { if(!m_terrainGenerator) return 0; float height=m_terrainGenerator->GenerateHeight(static_cast<int>(worldX/m_config.tileSize),static_cast<int>(worldZ/m_config.tileSize)); float temperature=m_terrainGenerator->GenerateTemperature(static_cast<int>(worldX/m_config.tileSize),static_cast<int>(worldZ/m_config.tileSize)); float humidity=m_terrainGenerator->GenerateHumidity(static_cast<int>(worldX/m_config.tileSize),static_cast<int>(worldZ/m_config.tileSize)); return static_cast<int>(m_biomeManager.GetDominantBiome(static_cast<int>(worldX),static_cast<int>(worldZ),height,temperature,humidity)); }
void World::GetTileSurface(int tileX, int tileZ, int& height, bool& water) const {
    int level = m_terrainGenerator->GenerateHeightLevel(tileX, tileZ);
    height = level;
    water  = level < TerrainGenerator::SEA_LEVEL;
    float flatH = 0.0f;
    if (GetFlattenedHeight(tileX + 0.5f, tileZ + 0.5f, flatH)) {
        height = (int)flatH;
        water  = false;
    }
}
void World::UpdateChunkLoading(Vector3 playerPosition){ UpdateChunkStreaming(playerPosition); }
void World::GenerateInitialChunks(){ constexpr int initialRadius=1; for(int x=-initialRadius;x<=initialRadius;x++) for(int z=-initialRadius;z<=initialRadius;z++) LoadChunkAt(x,z); TraceLog(LOG_INFO,"Generated %zu initial chunks",m_chunks.size()); }
bool World::ShouldLoadChunk(int chunkX,int chunkZ,Vector3 centerPos) const { if(IsChunkLoaded(chunkX,chunkZ)) return false; float distSquared=GetChunkDistanceSquared(chunkX,chunkZ,centerPos); float maxDistSquared=GENERATION_RADIUS*GENERATION_RADIUS; return distSquared<=maxDistSquared; }
bool World::ShouldUnloadChunk(int chunkX,int chunkZ,Vector3 centerPos) const { float distSquared=GetChunkDistanceSquared(chunkX,chunkZ,centerPos); float unloadDistSquared=UNLOAD_RADIUS*UNLOAD_RADIUS; return distSquared>unloadDistSquared; }

bool World::IsWaterAt(float worldX, float worldZ) const {
    int chunkX, chunkZ, localX, localZ;
    WorldToChunk(worldX, worldZ, chunkX, chunkZ, localX, localZ);
    Chunk* chunk = const_cast<World*>(this)->GetChunk(chunkX, chunkZ);
    if (chunk && chunk->IsGenerated()) {
        return chunk->GetTile(localX, localZ).hasWater;
    }
    if (!m_terrainGenerator) return false;

    // Stessa regola di Chunk::GenerateTerrain, senza passare da WorldFeatureGenerator.
    const int wx = (int)std::floor(worldX / m_config.tileSize);
    const int wz = (int)std::floor(worldZ / m_config.tileSize);
    float flatH;
    if (GetFlattenedHeight(wx + 0.5f, wz + 0.5f, flatH)) return false;
    return m_terrainGenerator->GenerateHeightLevel(wx, wz) < TerrainGenerator::SEA_LEVEL;
}
float World::GetChunkDistanceSquared(int chunkX,int chunkZ,Vector3 centerPos) const { int centerCX,centerCZ; WorldToChunk(centerPos.x,centerPos.z,centerCX,centerCZ); int dx=chunkX-centerCX; int dz=chunkZ-centerCZ; return (float)(dx*dx+dz*dz); }