#include "world/TerrainGenerator.h"
#include <cmath>
#include <raylib.h>
#include <raymath.h>

TerrainGenerator::TerrainGenerator()
    : m_seed(12345)
{
}

void TerrainGenerator::Initialize(unsigned int seed) {
    m_seed = seed;
}

float TerrainGenerator::GenerateHeight(int worldX, int worldZ) const {
    return HeightNoise(worldX, worldZ);
}

float TerrainGenerator::GenerateTemperature(int worldX, int worldZ) const {
    return TemperatureNoise(worldX, worldZ);
}

float TerrainGenerator::GenerateHumidity(int worldX, int worldZ) const {
    return HumidityNoise(worldX, worldZ);
}

Color TerrainGenerator::GenerateColor(float height, int biome) const {
    // Enhanced color generation based on height and biome
    if (height < -3.0f) {
        return BLUE;        // Deep water
    } else if (height < -1.0f) {
        return SKYBLUE;     // Shallow water
    } else if (height < 0.0f) {
        return BEIGE;       // Beach/sand
    } else if (height < 2.0f) {
        return GREEN;       // Grass
    } else if (height < 5.0f) {
        return DARKGREEN;   // Forest
    } else if (height < 8.0f) {
        return BROWN;       // Hills
    } else {
        return LIGHTGRAY;   // Mountains
    }
}

void TerrainGenerator::GenerateChunkData(int chunkX, int chunkZ, int chunkSize, float tileSize,
                                        float* heights, float* temperatures, float* humidity) const {
    for (int z = 0; z < chunkSize; z++) {
        for (int x = 0; x < chunkSize; x++) {
            int worldX = chunkX * chunkSize + x;
            int worldZ = chunkZ * chunkSize + z;
            int index = z * chunkSize + x;

            heights[index] = GenerateHeight(worldX, worldZ);
            temperatures[index] = GenerateTemperature(worldX, worldZ);
            humidity[index] = GenerateHumidity(worldX, worldZ);
            // Biome NOT produced here — single source is BiomeManager via World/Chunk pipeline
        }
    }
}

float TerrainGenerator::HeightNoise(int x, int z) const {
    return PerlinNoise(x * HEIGHT_FREQUENCY, z * HEIGHT_FREQUENCY, 0, HEIGHT_OCTAVES, HEIGHT_PERSISTENCE) * HEIGHT_AMPLITUDE;
}

float TerrainGenerator::TemperatureNoise(int x, int z) const {
    // Temperature decreases with latitude (z coordinate) and has some noise
    float latitudeEffect = 0.8f - (std::abs(z) * 0.0001f); // Colder at higher latitudes
    float noise = PerlinNoise(x * TEMP_FREQUENCY, z * TEMP_FREQUENCY, 1000, TEMP_OCTAVES, 0.5f) * TEMP_AMPLITUDE;
    return Clamp(latitudeEffect + noise * 0.3f, 0.0f, 1.0f);
}

float TerrainGenerator::HumidityNoise(int x, int z) const {
    return Clamp(PerlinNoise(x * HUMIDITY_FREQUENCY, z * HUMIDITY_FREQUENCY, 2000, HUMIDITY_OCTAVES, 0.5f) * HUMIDITY_AMPLITUDE + 0.5f, 0.0f, 1.0f);
}

float TerrainGenerator::BiomeNoise(int x, int z) const {
    return PerlinNoise(x * BIOME_FREQUENCY, z * BIOME_FREQUENCY, 3000, 2, 0.5f);
}

float TerrainGenerator::Noise(int x, int z, unsigned int seedOffset) const {
    // Enhanced hash-based noise with seed offset
    unsigned int hash = m_seed + seedOffset;
    hash ^= x * 374761393U;
    hash ^= z * 668265263U;
    hash = hash * 1664525U + 1013904223U;

    return (hash / (float)0xFFFFFFFFU) * 2.0f - 1.0f;
}

float TerrainGenerator::SmoothNoise(float x, float z, unsigned int seedOffset) const {
    // Get integer coordinates
    int intX = (int)x;
    int intZ = (int)z;

    // Get fractional parts
    float fracX = x - intX;
    float fracZ = z - intZ;

    // Get noise values at corners
    float v1 = Noise(intX, intZ, seedOffset);
    float v2 = Noise(intX + 1, intZ, seedOffset);
    float v3 = Noise(intX, intZ + 1, seedOffset);
    float v4 = Noise(intX + 1, intZ + 1, seedOffset);

    // Interpolate
    float i1 = Interpolate(v1, v2, fracX);
    float i2 = Interpolate(v3, v4, fracX);

    return Interpolate(i1, i2, fracZ);
}

float TerrainGenerator::InterpolatedNoise(float x, float z, unsigned int seedOffset) const {
    return SmoothNoise(x, z, seedOffset);
}

float TerrainGenerator::PerlinNoise(float x, float z, unsigned int seedOffset, int octaves, float persistence) const {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;

    for (int i = 0; i < octaves; i++) {
        total += InterpolatedNoise(x * frequency, z * frequency, seedOffset) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= 2.0f;
    }

    return total / maxValue;
}

float TerrainGenerator::Interpolate(float a, float b, float x) const {
    // Cosine interpolation for smoother results
    float ft = x * PI;
    float f = (1.0f - cosf(ft)) * 0.5f;
    return a * (1.0f - f) + b * f;
}