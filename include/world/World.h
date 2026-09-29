#pragma once

#include <raylib.h>
#include <vector>
#include <memory>
#include <unordered_map>

class Chunk;
class TerrainGenerator;
class BiomeManager;

struct WorldConfig {
    unsigned int seed = 12345;
    int chunkSize = 16;
    float tileSize = 1.0f;
    int renderDistance = 5;
};

class World {
public:
    World();
    ~World();

    bool Initialize(const WorldConfig& config);
    void Update(float deltaTime);
    void Render();

    void SetSeed(unsigned int seed);
    unsigned int GetSeed() const { return m_config.seed; }

    Chunk* GetChunk(int x, int z);
    void LoadChunkAt(int x, int z);
    void UnloadChunk(int x, int z);

    // Enhanced coordinate system
    void WorldToChunk(float worldX, float worldZ, int& chunkX, int& chunkZ) const;
    void WorldToChunk(float worldX, float worldZ, int& chunkX, int& chunkZ, int& localX, int& localZ) const;
    Vector3 ChunkToWorld(int chunkX, int chunkZ) const;
    Vector3 ChunkToWorldCenter(int chunkX, int chunkZ) const;

    // Chunk management
    void UpdateChunkStreaming(Vector3 centerPosition);
    void UnloadDistantChunks(Vector3 centerPosition);
    bool IsChunkLoaded(int chunkX, int chunkZ) const;
    int GetLoadedChunkCount() const { return static_cast<int>(m_chunks.size()); }

    // World queries
    float GetHeightAt(float worldX, float worldZ) const;
    int GetBiomeAt(float worldX, float worldZ) const;

    const WorldConfig& GetConfig() const { return m_config; }

private:
    struct ChunkKey {
        int x, z;

        bool operator==(const ChunkKey& other) const {
            return x == other.x && z == other.z;
        }
    };

    struct ChunkKeyHash {
        std::size_t operator()(const ChunkKey& key) const {
            return std::hash<int>()(key.x) ^ (std::hash<int>()(key.z) << 1);
        }
    };

    void UpdateChunkLoading(Vector3 playerPosition);
    void GenerateInitialChunks();
    bool ShouldLoadChunk(int chunkX, int chunkZ, Vector3 centerPos) const;
    bool ShouldUnloadChunk(int chunkX, int chunkZ, Vector3 centerPos) const;
    float GetChunkDistanceSquared(int chunkX, int chunkZ, Vector3 centerPos) const;

    WorldConfig m_config;
    std::unique_ptr<TerrainGenerator> m_terrainGenerator;
    std::unordered_map<ChunkKey, std::unique_ptr<Chunk>, ChunkKeyHash> m_chunks;

    Vector3 m_lastPlayerPosition;
    float m_lastUpdateTime;
    bool m_initialized;

    // Streaming parameters
    static constexpr float CHUNK_UPDATE_INTERVAL = 1.0f; // Update every second
    static constexpr float UNLOAD_DISTANCE_MULTIPLIER = 1.5f; // Unload chunks beyond render distance * this
};