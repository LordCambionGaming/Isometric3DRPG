#pragma once

#include <raylib.h>
#include <vector>
#include <memory>
#include <unordered_map>
#include "world/BiomeManager.h"

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
    bool IsInitialized() const { return m_initialized; }
    void Update(float deltaTime);
    void Render();

    void SetGlobalShader(Shader shader);

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

    void ForceDryArea(float centerX, float centerZ, float radius);
    struct FlattenArea { float x, z, radius, height; };
    std::vector<FlattenArea> m_flattenAreas;

    bool GetFlattenedHeight(float wx, float wz, float& outHeight) const;

    // Chunk management
    void UpdateChunkStreaming(Vector3 centerPosition);
    void SetPlayerPosition(Vector3 pos);
    void UpdateChunkLoading(Vector3 playerPosition);
    int GetGenerationRadius() const { return GENERATION_RADIUS; }
    int GetRenderRadius() const { return RENDER_RADIUS; }
    int GetUnloadRadius() const { return UNLOAD_RADIUS; }
    void UnloadDistantChunks(Vector3 centerPosition);
    bool IsChunkLoaded(int chunkX, int chunkZ) const;
    bool IsWaterAt(float worldX, float worldZ) const;
    int GetLoadedChunkCount() const { return static_cast<int>(m_chunks.size()); }

    // World queries
    float GetHeightAt(float worldX, float worldZ) const;
    int GetBiomeAt(float worldX, float worldZ) const;
    void GetTileSurface(int tileX, int tileZ, int& height, bool& water) const;

    const WorldConfig& GetConfig() const { return m_config; }

    static float VillageHash(float x, float z) {
    // rumore veloce pseudo-random
    float n = sinf(x * 12.9898f + z * 78.233f) * 43758.5453f;
    return n - floorf(n); // 0..1
    }
    static float VillageHashAngle(float cx, float cz, float angle) {
    // rumore stabile solo per angolo, non per ogni tile
    float a = sinf(angle * 3.7f) * 0.5f + 0.5f;
    float b = sinf(angle * 7.3f + cx * 0.1f) * 0.5f + 0.5f;
    return (a * 0.6f + b * 0.4f); // 0..1 stabile per angolo
}

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

    void GenerateInitialChunks();
    void ProcessPendingChunks();
    bool ShouldLoadChunk(int chunkX, int chunkZ, Vector3 centerPos) const;
    bool ShouldUnloadChunk(int chunkX, int chunkZ, Vector3 centerPos) const;
    float GetChunkDistanceSquared(int chunkX, int chunkZ, Vector3 centerPos) const;

    WorldConfig m_config;
    std::unique_ptr<TerrainGenerator> m_terrainGenerator;
    BiomeManager m_biomeManager;
    std::unordered_map<ChunkKey, std::unique_ptr<Chunk>, ChunkKeyHash> m_chunks;

    std::vector<ChunkKey> pendingChunks;
    static constexpr int MAX_CHUNKS_PER_FRAME = 1;

    Vector3 m_lastPlayerPosition;
    int m_lastChunkX = 0;
    int m_lastChunkZ = 0;
    float m_lastUpdateTime;
    float m_chunkLoadTimer = 0.0f;
    bool m_initialized;

    Shader m_globalShader;
    bool m_hasGlobalShader = false;

    // Streaming parameters: generation / render / unload separated for player-centered design
    static constexpr float CHUNK_UPDATE_INTERVAL = 0.5f;
    static constexpr float CHUNK_LOAD_INTERVAL = 0.12f;
    static constexpr float GENERATION_RADIUS = 4; // chunks around player to generate
    static constexpr float RENDER_RADIUS = 5;     // chunks visible
    static constexpr float UNLOAD_RADIUS = 7;     // chunks to unload (beyond this)
    static constexpr float UNLOAD_DISTANCE_MULTIPLIER = 1.5f;
};