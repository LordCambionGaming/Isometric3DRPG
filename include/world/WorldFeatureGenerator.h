#pragma once
#include <raylib.h>
#include <vector>

class TerrainGenerator;

class WorldFeatureGenerator {
public:
    WorldFeatureGenerator();
    void Initialize(unsigned int seed);

    // Feature types: 0=Dry, 1=Lake, 2=River
    int GetFeatureAt(int worldX, int worldZ, const TerrainGenerator& tg) const;
    bool IsRiver(int worldX, int worldZ, const TerrainGenerator& tg) const;
    bool IsLake(int worldX, int worldZ, const TerrainGenerator& tg) const;

private:
    unsigned int m_seed;
    float RiverShapeNoise(int x, int z) const;
    float RiverNarrowNoise(int x, int z) const;
    float LakeDepressionNoise(int x, int z) const;
};
