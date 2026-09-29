#include "world/World.h"
#include "world/Chunk.h"
#include "world/TerrainGenerator.h"
#include <iostream>
#include <cmath>

World::World()
    : m_terrainGenerator(nullptr)
    , m_lastPlayerPosition({0.0f, 0.0f, 0.0f})
    , m_lastUpdateTime(0.0f)
    , m_initialized(false)
{
}

World::~World() {
    // Chunks will be automatically cleaned up by unique_ptr
}

bool World::Initialize(const WorldConfig& config) {
    try {
        m_config = config;
        m_terrainGenerator = std::make_unique<TerrainGenerator>();
        m_terrainGenerator->Initialize(m_config.seed);
        m_lastUpdateTime = 0.0f;

        GenerateInitialChunks();

        m_initialized = true;
        std::cout << "World initialized with seed: " << m_config.seed << std::endl;
        std::cout << "Chunk size: " << m_config.chunkSize << "x" << m_config.chunkSize << std::endl;
        std::cout << "Render distance: " << m_config.renderDistance << " chunks" << std::endl;

        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in World::Initialize: " << e.what() << std::endl;
        return false;
    }
}

void World::Update(float deltaTime) {
    if (!m_initialized) return;

    m_lastUpdateTime += deltaTime;

    // Update chunk streaming periodically
    if (m_lastUpdateTime >= CHUNK_UPDATE_INTERVAL) {
        Vector3 currentPosition = {0.0f, 0.0f, 0.0f}; // Player position placeholder
        UpdateChunkStreaming(currentPosition);
        m_lastUpdateTime = 0.0f;
    }
}

void World::Render() {
    if (!m_initialized) return;

    // Render all loaded chunks
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

    // Clear existing chunks and regenerate
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

    // Check if chunk already exists
    if (m_chunks.find(key) != m_chunks.end()) {
        return;
    }

    // Create new chunk
    auto chunk = std::make_unique<Chunk>(x, z, m_config.chunkSize, m_config.tileSize);
    chunk->Generate(m_config.seed);
    chunk->BuildMesh();

    m_chunks[key] = std::move(chunk);
}

void World::UnloadChunk(int x, int z) {
    ChunkKey key = {x, z};
    auto it = m_chunks.find(key);

    if (it != m_chunks.end()) {
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

    // Calculate local coordinates within the chunk
    float localWorldX = worldX - (chunkX * chunkWorldSize);
    float localWorldZ = worldZ - (chunkZ * chunkWorldSize);

    localX = static_cast<int>(std::floor(localWorldX / m_config.tileSize));
    localZ = static_cast<int>(std::floor(localWorldZ / m_config.tileSize));

    // Clamp to chunk bounds
    localX = std::max(0, std::min(localX, m_config.chunkSize - 1));
    localZ = std::max(0, std::min(localZ, m_config.chunkSize - 1));
}

Vector3 World::ChunkToWorld(int chunkX, int chunkZ) const {
    float chunkWorldSize = m_config.chunkSize * m_config.tileSize;
    return {
        chunkX * chunkWorldSize,
        0.0f,
        chunkZ * chunkWorldSize
    };
}

Vector3 World::ChunkToWorldCenter(int chunkX, int chunkZ) const {
    float chunkWorldSize = m_config.chunkSize * m_config.tileSize;
    return {
        chunkX * chunkWorldSize + chunkWorldSize * 0.5f,
        0.0f,
        chunkZ * chunkWorldSize + chunkWorldSize * 0.5f
    };
}

void World::UpdateChunkStreaming(Vector3 centerPosition) {
    // Load chunks around the center position
    int centerChunkX, centerChunkZ;
    WorldToChunk(centerPosition.x, centerPosition.z, centerChunkX, centerChunkZ);

    // Load new chunks in render distance
    for (int x = centerChunkX - m_config.renderDistance; x <= centerChunkX + m_config.renderDistance; x++) {
        for (int z = centerChunkZ - m_config.renderDistance; z <= centerChunkZ + m_config.renderDistance; z++) {
            if (ShouldLoadChunk(x, z, centerPosition)) {
                LoadChunkAt(x, z);
            }
        }
    }

    // Unload distant chunks
    UnloadDistantChunks(centerPosition);

    m_lastPlayerPosition = centerPosition;
}

void World::UnloadDistantChunks(Vector3 centerPosition) {
    std::vector<ChunkKey> chunksToUnload;

    for (const auto& pair : m_chunks) {
        const ChunkKey& key = pair.first;
        if (ShouldUnloadChunk(key.x, key.z, centerPosition)) {
            chunksToUnload.push_back(key);
        }
    }

    for (const ChunkKey& key : chunksToUnload) {
        UnloadChunk(key.x, key.z);
    }

    if (!chunksToUnload.empty()) {
        std::cout << "Unloaded " << chunksToUnload.size() << " distant chunks" << std::endl;
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

    // Try to get from loaded chunk first
    Chunk* chunk = const_cast<World*>(this)->GetChunk(chunkX, chunkZ);
    if (chunk && chunk->IsGenerated()) {
        return chunk->GetHeightAt(localX * m_config.tileSize, localZ * m_config.tileSize);
    }

    // Fallback to terrain generator
    return m_terrainGenerator->GenerateHeight(static_cast<int>(worldX / m_config.tileSize),
                                            static_cast<int>(worldZ / m_config.tileSize));
}

int World::GetBiomeAt(float worldX, float worldZ) const {
    if (!m_terrainGenerator) return 0;

    return m_terrainGenerator->GenerateBiome(static_cast<int>(worldX / m_config.tileSize),
                                           static_cast<int>(worldZ / m_config.tileSize));
}

void World::UpdateChunkLoading(Vector3 playerPosition) {
    // Legacy method - now delegates to UpdateChunkStreaming
    UpdateChunkStreaming(playerPosition);
}

void World::GenerateInitialChunks() {
    // Generate a small grid of chunks around the origin
    int halfSize = m_config.renderDistance;

    for (int x = -halfSize; x <= halfSize; x++) {
        for (int z = -halfSize; z <= halfSize; z++) {
            LoadChunkAt(x, z);
        }
    }

    std::cout << "Generated " << m_chunks.size() << " initial chunks" << std::endl;
}

bool World::ShouldLoadChunk(int chunkX, int chunkZ, Vector3 centerPos) const {
    // Don't load if already loaded
    if (IsChunkLoaded(chunkX, chunkZ)) return false;

    // Check distance
    float distSquared = GetChunkDistanceSquared(chunkX, chunkZ, centerPos);
    float maxDistSquared = m_config.renderDistance * m_config.renderDistance;

    return distSquared <= maxDistSquared;
}

bool World::ShouldUnloadChunk(int chunkX, int chunkZ, Vector3 centerPos) const {
    float distSquared = GetChunkDistanceSquared(chunkX, chunkZ, centerPos);
    float unloadDistSquared = (m_config.renderDistance * UNLOAD_DISTANCE_MULTIPLIER);
    unloadDistSquared *= unloadDistSquared;

    return distSquared > unloadDistSquared;
}

float World::GetChunkDistanceSquared(int chunkX, int chunkZ, Vector3 centerPos) const {
    Vector3 chunkCenter = ChunkToWorldCenter(chunkX, chunkZ);
    float dx = chunkCenter.x - centerPos.x;
    float dz = chunkCenter.z - centerPos.z;

    return dx * dx + dz * dz;
}