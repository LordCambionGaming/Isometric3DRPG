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
    int GenerateBiome(int worldX, int worldZ) const;

    void SetSeed(unsigned int seed) { m_seed = seed; }
    unsigned int GetSeed() const { return m_seed; }

    // Chunk-specific generation
    void GenerateChunkData(int chunkX, int chunkZ, int chunkSize, float tileSize,
                          float* heights, float* temperatures, float* humidity, int* biomes) const;

private:
    // Multi-layer noise generation
    float HeightNoise(int x, int z) const;
    float TemperatureNoise(int x, int z) const;
    float HumidityNoise(int x, int z) const;
    float BiomeNoise(int x, int z) const;

    // Core noise functions
    float Noise(int x, int z, unsigned int seedOffset) const;
    float SmoothNoise(float x, float z, unsigned int seedOffset) const;
    float InterpolatedNoise(float x, float z, unsigned int seedOffset) const;
    float PerlinNoise(float x, float z, unsigned int seedOffset, int octaves, float persistence) const;

    float Interpolate(float a, float b, float x) const;

    unsigned int m_seed;

    // Terrain generation parameters
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