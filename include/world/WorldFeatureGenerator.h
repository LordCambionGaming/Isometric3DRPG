#pragma once
#include <raylib.h>
#include <vector>

class TerrainGenerator;

enum class WaterFeature { None = 0, Lake = 1, River = 2, Sea = 3 };

class WorldFeatureGenerator {
public:
    WorldFeatureGenerator();
    void Initialize(unsigned int seed);

    // Deterministic on worldX,worldZ, seed — no chunk state
    WaterFeature GetFeatureAt(int worldX, int worldZ, const TerrainGenerator& tg) const;
    bool IsSea(int worldX, int worldZ, const TerrainGenerator& tg) const;
    bool IsLake(int worldX, int worldZ, const TerrainGenerator& tg) const;
    bool IsRiver(int worldX, int worldZ, const TerrainGenerator& tg) const;

private:
    unsigned int m_seed;
    // Large-scale continentalness / landmass noise
    float ContinentalNoise(int x, int z) const;
    // High-scale lake depression / basin
    float LakeDepressionNoise(int x, int z) const;
    // Continuous river path noise
    float RiverPathNoise(int x, int z) const;
    float RiverNarrowNoise(int x, int z) const;
};
