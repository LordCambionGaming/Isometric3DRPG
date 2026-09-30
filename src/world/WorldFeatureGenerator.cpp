#include "world/WorldFeatureGenerator.h"
#include "world/TerrainGenerator.h"
#include <cmath>

WorldFeatureGenerator::WorldFeatureGenerator() : m_seed(12345) {}
void WorldFeatureGenerator::Initialize(unsigned int seed) { m_seed = seed; }

int WorldFeatureGenerator::GetFeatureAt(int worldX, int worldZ, const TerrainGenerator& tg) const {
    int h = tg.GenerateHeightLevel(worldX, worldZ);
    if (IsRiver(worldX, worldZ, tg)) return 2;
    if (IsLake(worldX, worldZ, tg)) return 1;
    return 0;
}

bool WorldFeatureGenerator::IsRiver(int worldX, int worldZ, const TerrainGenerator& tg) const {
    float shape = tg.GenerateHeight(worldX * 0.008f, worldZ * 0.008f);
    float narrow = tg.GenerateHeight(worldX * 0.06f + 500.0f, worldZ * 0.06f + 500.0f);
    int h = tg.GenerateHeightLevel(worldX, worldZ);
    return (shape > 0.35f && shape < 0.65f) && (std::abs(narrow) < 0.15f) && (h <= 3);
}

bool WorldFeatureGenerator::IsLake(int worldX, int worldZ, const TerrainGenerator& tg) const {
    int h = tg.GenerateHeightLevel(worldX, worldZ);
    float region = tg.GetRegion(worldX, worldZ);
    float dep = tg.GenerateHeight(worldX * 0.03f + 100.0f, worldZ * 0.03f + 100.0f);
    return (h == 1) && (region == 0 || region == 4) && (dep < 0.25f);
}
