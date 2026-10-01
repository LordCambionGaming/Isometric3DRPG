#include "world/World.h"
#include "world/Chunk.h"
#include "world/TerrainGenerator.h"
#include <iostream>
#include <cmath>
#include <chrono>
#include <algorithm>

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

bool World::Initialize(const WorldConfig& config) {
    try {
        m_config = config;
        m_terrainGenerator = std::make_unique<TerrainGenerator>();
        m_terrainGenerator->Initialize(m_config.seed);
        m_lastUpdateTime = 0.0f;

        GenerateInitialChunks();
        m_biomeManager.Initialize();

        m_initialized = true;
        std::cout << "World initialized with seed: " << m_config.seed << std::endl;
        std::cout << "Chunk size: " << m_config.chunkSize << "x" << m_config.chunkSize << std::endl;

        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in World::Initialize: " << e.what() << std::endl;
        return false;
    }
}

void World::SetPlayerPosition(Vector3 pos) {
    m_lastPlayerPosition = pos;
}

void World::Update(float deltaTime) {
    if (!m_initialized) return;
    std::cout << "[WORLD] PLAYER POSITION AT UPDATE=(" << m_lastPlayerPosition.x << "," << m_lastPlayerPosition.y << "," << m_lastPlayerPosition.z << ")" << std::endl;

    // Use Game-provided player position if available; fallback to last known
    Vector3 currentPosition = m_lastPlayerPosition;

    int cx, cz;
    WorldToChunk(currentPosition.x, currentPosition.z, cx, cz);

    bool chunkChanged = (cx != m_lastChunkX || cz != m_lastChunkZ);

    if (chunkChanged || m_lastUpdateTime >= CHUNK_UPDATE_INTERVAL) {
        if (chunkChanged) {
            std::cout << "[STREAM CENTER CHANGE] old=(" << m_lastChunkX << "," << m_lastChunkZ << ") new=(" << cx << "," << cz << ")" << std::endl;
            m_lastChunkX = cx;
            m_lastChunkZ = cz;
        }

        UpdateChunkStreaming(currentPosition);

        std::cout << "[STREAM] Loaded chunks: " << m_chunks.size() << std::endl;

        m_lastUpdateTime = 0.0f;
    }
    else {
        m_lastUpdateTime += deltaTime;
    }

    // Process pending chunk queue (bounded work per frame)
    ProcessPendingChunks();
}

void World::Render() {
    if (!m_initialized) return;
    for (const auto& pair : m_chunks) {
        const auto& chunk = pair.second;
        if (chunk && chunk->IsGenerated() && chunk->HasMesh()) {
            chunk->Render();
        }
    }
}

void World::SetSeed(unsigned int seed) {
    if (m_config.seed == seed) return;

    m_config.seed = seed;

    if (m_terrainGenerator) {
        m_terrainGenerator->SetSeed(seed);
    }

    m_chunks.clear();
    GenerateInitialChunks();

    std::cout << "World regenerated with seed: " << seed << std::endl;
    std::cout << "Generated " << m_chunks.size() << " chunks" << std::endl;
}

Chunk* World::GetChunk(int x, int z) {
    ChunkKey key = {x, z};
    auto it = m_chunks.find(key);
    return (it != m_chunks.end()) ? it->second.get() : nullptr;
}

void World::LoadChunkAt(int x, int z) {
    ChunkKey key = {x, z};

    if (m_chunks.find(key) != m_chunks.end()) return;

    std::cout << "[STREAM] Loading chunk (" << x << "," << z << ")" << std::endl;

    auto startGen = std::chrono::high_resolution_clock::now();
    auto chunk = std::make_unique<Chunk>(x, z, m_config.chunkSize, m_config.tileSize);
    chunk->Generate(*m_terrainGenerator, this);

    auto endGen = std::chrono::high_resolution_clock::now();
    auto genMs = std::chrono::duration_cast<std::chrono::milliseconds>(endGen - startGen).count();
    std::cout << "[STREAM] Generate chunk (" << x << "," << z << ") took " << genMs << "ms" << std::endl;

    auto startMesh = std::chrono::high_resolution_clock::now();
    chunk->BuildMesh();
    auto endMesh = std::chrono::high_resolution_clock::now();
    auto meshMs = std::chrono::duration_cast<std::chrono::milliseconds>(endMesh - startMesh).count();
    std::cout << "[STREAM] BuildMesh chunk (" << x << "," << z << ") took " << meshMs << "ms" << std::endl;

    m_chunks[key] = std::move(chunk);
    std::cout << "[STREAM] Loaded chunk (" << x << "," << z << ") total=" << m_chunks.size() << std::endl;
}

void World::ProcessPendingChunks() {
    int processed = 0;
    auto frameStart = std::chrono::high_resolution_clock::now();

    while (processed < MAX_CHUNKS_PER_FRAME && !pendingChunks.empty()) {
        ChunkKey k = pendingChunks.front();
        pendingChunks.erase(pendingChunks.begin());

        // Skip if already loaded (could have been loaded via other path)
        if (IsChunkLoaded(k.x, k.z)) continue;

        // Load with profiling
        std::cout << "[CHUNK LOAD] chunk=(" << k.x << "," << k.z << ")" << std::endl;
        auto genStart = std::chrono::high_resolution_clock::now();
        auto chunk = std::make_unique<Chunk>(k.x, k.z, m_config.chunkSize, m_config.tileSize);
        chunk->Generate(*m_terrainGenerator, this);
        auto genEnd = std::chrono::high_resolution_clock::now();
        long genMs = std::chrono::duration_cast<std::chrono::milliseconds>(genEnd - genStart).count();

        auto meshStart = std::chrono::high_resolution_clock::now();
        chunk->BuildMesh();
        auto meshEnd = std::chrono::high_resolution_clock::now();
        long meshMs = std::chrono::duration_cast<std::chrono::milliseconds>(meshEnd - meshStart).count();

        std::cout << "[CHUNK GENERATED] chunk=(" << k.x << "," << k.z << ")" << std::endl;

        ChunkKey key = {k.x, k.z};
        std::cout << "[STREAM PROCESS] chunk=(" << k.x << "," << k.z << ")" << std::endl;
        m_chunks[key] = std::move(chunk);

        std::cout << "[STREAM PERF] Generate chunk (" << k.x << "," << k.z << "): " << genMs << " ms" << std::endl;
        std::cout << "[STREAM PERF] BuildMesh chunk (" << k.x << "," << k.z << "): " << meshMs << " ms" << std::endl;

        processed++;
    }

    if (processed > 0) {
        auto frameEnd = std::chrono::high_resolution_clock::now();
        long frameMs = std::chrono::duration_cast<std::chrono::milliseconds>(frameEnd - frameStart).count();
        std::cout << "[STREAM PERF] processed=" << processed << " queued=" << pendingChunks.size()
                  << " frameTime=" << frameMs << " ms" << std::endl;
    }
}

void World::UnloadChunk(int x, int z) {
    ChunkKey key = {x, z};
    auto it = m_chunks.find(key);

    if (it != m_chunks.end()) {
        std::cout << "[STREAM] Unloaded chunk (" << x << "," << z << ") distance="
                  << (int)std::sqrt((float)(x - m_lastChunkX)*(x - m_lastChunkX) + (z - m_lastChunkZ)*(z - m_lastChunkZ)) << std::endl;
        it->second->Unload();
        m_chunks.erase(it);
    }
}

void World::WorldToChunk(float worldX, float worldZ, int& chunkX, int& chunkZ) const {
    float chunkWorldSize = m_config.chunkSize * m_config.tileSize;
    chunkX = static_cast<int>(std::floor(worldX / chunkWorldSize));
    chunkZ = static_cast<int>(std::floor(worldZ / chunkWorldSize));
}

void World::WorldToChunk(float worldX, float worldZ, int& chunkX, int& chunkZ, int& localX, int& localZ) const {
    float chunkWorldSize = m_config.chunkSize * m_config.tileSize;
    chunkX = static_cast<int>(std::floor(worldX / chunkWorldSize));
    chunkZ = static_cast<int>(std::floor(worldZ / chunkWorldSize));

    float localWorldX = worldX - (chunkX * chunkWorldSize);
    float localWorldZ = worldZ - (chunkZ * chunkWorldSize);

    localX = static_cast<int>(std::floor(localWorldX / m_config.tileSize));
    localZ = static_cast<int>(std::floor(localWorldZ / m_config.tileSize));

    localX = std::max(0, std::min(localX, m_config.chunkSize - 1));
    localZ = std::max(0, std::min(localZ, m_config.chunkSize - 1));
}

Vector3 World::ChunkToWorld(int chunkX, int chunkZ) const {
    float chunkWorldSize = m_config.chunkSize * m_config.tileSize;
    return { chunkX * chunkWorldSize, 0.0f, chunkZ * chunkWorldSize };
}

Vector3 World::ChunkToWorldCenter(int chunkX, int chunkZ) const {
    float chunkWorldSize = m_config.chunkSize * m_config.tileSize;
    return { chunkX * chunkWorldSize + chunkWorldSize * 0.5f, 0.0f, chunkZ * chunkWorldSize + chunkWorldSize * 0.5f };
}

void World::UpdateChunkStreaming(Vector3 centerPosition) {
    int centerChunkX, centerChunkZ;
    WorldToChunk(centerPosition.x, centerPosition.z, centerChunkX, centerChunkZ);

    std::cout << "[STREAM] Player position = (" << centerPosition.x << "," << centerPosition.y << "," << centerPosition.z << ")" << std::endl;
    std::cout << "[STREAM] Player chunk = (" << centerChunkX << "," << centerChunkZ << ")" << std::endl;

    // Update pending queue: add missing chunks, remove invalid ones, sort by distance
    int before = static_cast<int>(m_chunks.size());
    std::cout << "[STREAM] Loaded chunks BEFORE = " << before << std::endl;

    // 1. Compute needed chunks within GENERATION_RADIUS
    std::vector<ChunkKey> needed;
    for (int x = centerChunkX - GENERATION_RADIUS; x <= centerChunkX + GENERATION_RADIUS; x++) {
        for (int z = centerChunkZ - GENERATION_RADIUS; z <= centerChunkZ + GENERATION_RADIUS; z++) {
            if (GetChunkDistanceSquared(x, z, centerPosition) <= GENERATION_RADIUS * GENERATION_RADIUS) {
                needed.push_back({x, z});
            }
        }
    }

    // 2. Remove from pending those already loaded or no longer needed
    std::vector<ChunkKey> cleaned;
    for (const ChunkKey& k : pendingChunks) {
        bool inChunks = IsChunkLoaded(k.x, k.z);
        bool stillNeeded = false;
        for (const ChunkKey& need : needed) {
            if (need.x == k.x && need.z == k.z) { stillNeeded = true; break; }
        }
        if (!inChunks && stillNeeded) cleaned.push_back(k);
    }
    pendingChunks = cleaned;

    // 3. Add missing needed chunks (deduplicate against chunks and pending)
    for (const ChunkKey& k : needed) {
        bool alreadyInChunks = IsChunkLoaded(k.x, k.z);
        bool alreadyPending = false;
        for (const ChunkKey& p : pendingChunks) {
            if (p.x == k.x && p.z == k.z) { alreadyPending = true; break; }
        }
        if (!alreadyInChunks && !alreadyPending) {
            std::cout << "[STREAM QUEUE] chunk=(" << k.x << "," << k.z << ")" << std::endl;
            pendingChunks.push_back(k);
        }
    }

    // 4. Sort by distance to center (ascending) — closest first
    std::sort(pendingChunks.begin(), pendingChunks.end(),
        [centerChunkX, centerChunkZ](const ChunkKey& a, const ChunkKey& b) {
            int da = (a.x - centerChunkX)*(a.x - centerChunkX) + (a.z - centerChunkZ)*(a.z - centerChunkZ);
            int db = (b.x - centerChunkX)*(b.x - centerChunkX) + (b.z - centerChunkZ)*(b.z - centerChunkZ);
            if (da != db) return da < db;
            if (a.x != b.x) return a.x < b.x;
            return a.z < b.z;
        });

    std::cout << "[STREAM PERF] queued=" << pendingChunks.size() << std::endl;

    // Unload distant chunks
    UnloadDistantChunks(centerPosition);

    int after = static_cast<int>(m_chunks.size());
    std::cout << "[STREAM] Loaded chunks AFTER = " << after << std::endl;

    m_lastPlayerPosition = centerPosition;
}

void World::UnloadDistantChunks(Vector3 centerPosition) {
    int centerCX, centerCZ;
    WorldToChunk(centerPosition.x, centerPosition.z, centerCX, centerCZ);

    std::vector<ChunkKey> chunksToUnload;

    for (const auto& pair : m_chunks) {
        const ChunkKey& key = pair.first;
        int dx = key.x - centerCX;
        int dz = key.z - centerCZ;
        float dSq = (float)(dx * dx + dz * dz);

        // Unload if beyond UNLOAD_RADIUS (chunk-space distance)
        if (dSq > UNLOAD_RADIUS * UNLOAD_RADIUS) {
            chunksToUnload.push_back(key);
        }
    }

    for (const ChunkKey& key : chunksToUnload) {
        std::cout << "[CHUNK UNLOAD] chunk=(" << key.x << "," << key.z << ")" << std::endl;
        UnloadChunk(key.x, key.z);
    }

    if (!chunksToUnload.empty()) {
        std::cout << "[STREAM] Unloaded " << chunksToUnload.size() << " distant chunks" << std::endl;
    }
}

bool World::IsChunkLoaded(int chunkX, int chunkZ) const {
    ChunkKey key = {chunkX, chunkZ};
    return m_chunks.find(key) != m_chunks.end();
}

float World::GetHeightAt(float worldX, float worldZ) const {
    if (!m_terrainGenerator) return 0.0f;

    int chunkX, chunkZ, localX, localZ;
    WorldToChunk(worldX, worldZ, chunkX, chunkZ, localX, localZ);

    Chunk* chunk = const_cast<World*>(this)->GetChunk(chunkX, chunkZ);
    if (chunk && chunk->IsGenerated()) {
        const TerrainTile& tile = chunk->GetTile(localX, localZ);
        // GetHeightAt() returns the visible surface; water has its own surface height.
        if (tile.hasWater) {
            return tile.waterHeight;
        }
        return tile.height;
    }

    // Replica deterministica della generazione chunk (stessa logica di Chunk::GenerateTerrain, senza chunk)
    int worldXInt = static_cast<int>(worldX / m_config.tileSize);
    int worldZInt = static_cast<int>(worldZ / m_config.tileSize);
    int level = m_terrainGenerator->GenerateHeightLevel(worldXInt, worldZInt);

    // Replica feature water (stesso seed, stessa API usata in Chunk::Generate)
    WorldFeatureGenerator featureGen;
    featureGen.Initialize(m_terrainGenerator->GetSeed());
    WaterFeature wf = featureGen.GetFeatureAt(worldXInt, worldZInt, *m_terrainGenerator);
    bool isWater = (wf == WaterFeature::Sea) || (wf == WaterFeature::Lake) || (wf == WaterFeature::River);

    // Regola sea: se acqua -> surface = waterHeight = 2 (SEA_LEVEL)
    if (isWater) {
        return 2.0f; // waterHeight / SEA_LEVEL
    }
    // Land: final height con clamp >= 2 (gi? applicato da GenerateHeightLevel ma verificato)
    float h = static_cast<float>(level);
    if (h < 2.0f) h = 2.0f;
    return h;
}

int World::GetBiomeAt(float worldX, float worldZ) const {
    if (!m_terrainGenerator) return 0;
    float height = m_terrainGenerator->GenerateHeight(static_cast<int>(worldX / m_config.tileSize), static_cast<int>(worldZ / m_config.tileSize));
    float temperature = m_terrainGenerator->GenerateTemperature(static_cast<int>(worldX / m_config.tileSize), static_cast<int>(worldZ / m_config.tileSize));
    float humidity = m_terrainGenerator->GenerateHumidity(static_cast<int>(worldX / m_config.tileSize), static_cast<int>(worldZ / m_config.tileSize));
    return static_cast<int>(m_biomeManager.GetDominantBiome(static_cast<int>(worldX), static_cast<int>(worldZ), height, temperature, humidity));
}

void World::UpdateChunkLoading(Vector3 playerPosition) {
    // Keep for compatibility; streaming handled internally by Update()
    UpdateChunkStreaming(playerPosition);
}

void World::GenerateInitialChunks() {
    int halfSize = GENERATION_RADIUS;

    for (int x = -halfSize; x <= halfSize; x++) {
        for (int z = -halfSize; z <= halfSize; z++) {
            LoadChunkAt(x, z);
        }
    }

    std::cout << "Generated " << m_chunks.size() << " initial chunks (radius=" << GENERATION_RADIUS << ")" << std::endl;
}

bool World::ShouldLoadChunk(int chunkX, int chunkZ, Vector3 centerPos) const {
    if (IsChunkLoaded(chunkX, chunkZ)) return false;

    float distSquared = GetChunkDistanceSquared(chunkX, chunkZ, centerPos);
    float maxDistSquared = GENERATION_RADIUS * GENERATION_RADIUS;

    return distSquared <= maxDistSquared;
}

bool World::ShouldUnloadChunk(int chunkX, int chunkZ, Vector3 centerPos) const {
    float distSquared = GetChunkDistanceSquared(chunkX, chunkZ, centerPos);
    float unloadDistSquared = UNLOAD_RADIUS * UNLOAD_RADIUS;
    return distSquared > unloadDistSquared;
}

bool World::IsWaterAt(float worldX, float worldZ) const {
    int chunkX, chunkZ, localX, localZ;
    WorldToChunk(worldX, worldZ, chunkX, chunkZ, localX, localZ);
    Chunk* chunk = const_cast<World*>(this)->GetChunk(chunkX, chunkZ);
    if (chunk && chunk->IsGenerated()) {
        // Single source of truth: TileData.hasWater (not height / color / coord)
        return chunk->GetTile(localX, localZ).hasWater;
    }
    // Replica deterministica senza chunk (stessa logica Chunk::GenerateTerrain)
    int wx = static_cast<int>(worldX / m_config.tileSize);
    int wz = static_cast<int>(worldZ / m_config.tileSize);
    if (!m_terrainGenerator) return false;
    WorldFeatureGenerator featureGen;
    featureGen.Initialize(m_terrainGenerator->GetSeed());
    WaterFeature wf = featureGen.GetFeatureAt(wx, wz, *m_terrainGenerator);
    bool isWater = (wf == WaterFeature::Sea) || (wf == WaterFeature::Lake) || (wf == WaterFeature::River);
    return isWater;
}

float World::GetChunkDistanceSquared(int chunkX, int chunkZ, Vector3 centerPos) const {
    // Chunk-space distance: compare chunk coordinates directly
    int centerCX, centerCZ;
    WorldToChunk(centerPos.x, centerPos.z, centerCX, centerCZ);
    int dx = chunkX - centerCX;
    int dz = chunkZ - centerCZ;
    return (float)(dx * dx + dz * dz);
}
