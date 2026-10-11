#pragma once

#include <raylib.h>

class TerrainGenerator {
public:
    TerrainGenerator();
    ~TerrainGenerator() = default;

    void Initialize(unsigned int seed);

    float GenerateHeight(int worldX, int worldZ) const;
    float GenerateTemperature(int worldX, int worldZ) const;
    float GenerateHumidity(int worldX, int worldZ) const;
    Color GenerateColor(float height, int biome) const;

    void SetSeed(unsigned int seed) { m_seed = seed; }
    unsigned int GetSeed() const { return m_seed; }

    int GenerateHeightLevel(int worldX, int worldZ) const;
    int GetRegion(int worldX, int worldZ) const;

    // Chunk-specific generation
    void GenerateChunkData(int chunkX, int chunkZ, int chunkSize, float tileSize,
                          float* heights, float* temperatures, float* humidity) const;

   static constexpr int SEA_LEVEL = 4; // y del pelo dell'acqua (in blocchi)

private:
    // Region types for terrain generation
    enum class TerrainRegion {
        Plains,
        Hills,
        Plateau,
        Mountains,
        Lowlands
    };

    // Multi-layer noise generation
    float HeightNoise(int x, int z) const;
    float TemperatureNoise(int x, int z) const;
    float HumidityNoise(int x, int z) const;
    float BiomeNoise(int x, int z) const;
    float RegionNoise(int x, int z) const;
    float ShapeNoise(int x, int z) const;
    float DetailNoise(int x, int z) const;

    // Core noise functions
    float Noise(int x, int z, unsigned int seedOffset) const;
    float SmoothNoise(float x, float z, unsigned int seedOffset) const;
    float InterpolatedNoise(float x, float z, unsigned int seedOffset) const;
    float PerlinNoise(float x, float z, unsigned int seedOffset, int octaves, float persistence) const;

    float Interpolate(float a, float b, float x) const;

    unsigned int m_seed;

    // Terrain generation parameters
    static constexpr float REGION_FREQUENCY = 0.003f;
    static constexpr float SHAPE_FREQUENCY  = 0.035f;
    static constexpr float DETAIL_FREQUENCY = 0.050f;

    // Legacy terrain generation parameters (kept for existing methods)
    static constexpr float HEIGHT_FREQUENCY = 0.015f;
    static constexpr float HEIGHT_AMPLITUDE = 12.0f;
    static constexpr int HEIGHT_OCTAVES = 5;
    static constexpr float HEIGHT_PERSISTENCE = 0.6f;

    // Temperature parameters
    static constexpr float TEMP_FREQUENCY = 0.008f;
    static constexpr float TEMP_AMPLITUDE = 1.0f;
    static constexpr int TEMP_OCTAVES = 3;

    // Humidity parameters
    static constexpr float HUMIDITY_FREQUENCY = 0.012f;
    static constexpr float HUMIDITY_AMPLITUDE = 1.0f;
    static constexpr int HUMIDITY_OCTAVES = 3;

    // Biome parameters
    static constexpr float BIOME_FREQUENCY = 0.005f;
};