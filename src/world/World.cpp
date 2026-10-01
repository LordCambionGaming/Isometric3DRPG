#include "world/World.h"
#include "world/Chunk.h"
#include "world/TerrainGenerator.h"
#include <iostream>
#include <cmath>
#include <chrono>

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

    // Use Game-provided player position if available; fallback to last known
    Vector3 currentPosition = m_lastPlayerPosition;

    int cx, cz;
    WorldToChunk(currentPosition.x, currentPosition.z, cx, cz);

    bool chunkChanged = (cx != m_lastChunkX || cz != m_lastChunkZ);

    if (chunkChanged || m_lastUpdateTime >= CHUNK_UPDATE_INTERVAL) {
        if (chunkChanged) {
            std::cout << "[STREAM] Player chunk changed: (" << m_lastChunkX << "," << m_lastChunkZ << ") -> (" << cx << "," << cz << ")" << std::endl;
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
    chunk->Generate(*m_terrainGenerator);

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

void World::UnloadChunk(int x, int z) {
    ChunkKey key = {x, z};
    auto it = m_chunks.find(key);

    if (it != m_chunks.end()) {
        std::cout << "[STREAM] Unloaded chunk (" << x << "," << z << ")" << std::endl;
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

    int before = static_cast<int>(m_chunks.size());
    std::cout << "[STREAM] Loaded chunks BEFORE = " << before << std::endl;

    // Load new chunks in GENERATION_RADIUS (chunk space, not world units)
    for (int x = centerChunkX - GENERATION_RADIUS; x <= centerChunkX + GENERATION_RADIUS; x++) {
        for (int z = centerChunkZ - GENERATION_RADIUS; z <= centerChunkZ + GENERATION_RADIUS; z++) {
            if (ShouldLoadChunk(x, z, centerPosition)) {
                LoadChunkAt(x, z);
            }
        }
    }

    // Unload chunks beyond UNLOAD_RADIUS (chunk-space distance)
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
        return chunk->GetHeightAt(localX * m_config.tileSize, localZ * m_config.tileSize);
    }

    return m_terrainGenerator->GenerateHeight(static_cast<int>(worldX / m_config.tileSize),
                                            static_cast<int>(worldZ / m_config.tileSize));
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

float World::GetChunkDistanceSquared(int chunkX, int chunkZ, Vector3 centerPos) const {
    // Chunk-space distance: compare chunk coordinates directly
    int centerCX, centerCZ;
    WorldToChunk(centerPos.x, centerPos.z, centerCX, centerCZ);
    int dx = chunkX - centerCX;
    int dz = chunkZ - centerCZ;
    return (float)(dx * dx + dz * dz);
}
