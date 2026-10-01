#include "world/WorldFeatureGenerator.h"
#include "world/TerrainGenerator.h"
#include <cmath>

// Parameters (easy to change)
static constexpr float CONTINENTAL_FREQUENCY = 0.003f;
static constexpr float CONTINENTAL_THRESHOLD = -0.28f;
static constexpr float SEA_LEVEL = 2.0f;

static constexpr float LAKE_FREQ = 0.03f;
static constexpr float LAKE_THRESHOLD = -0.20f;

static constexpr float RIVER_FREQ = 0.008f;
static constexpr float RIVER_NARROW = 0.06f;
static constexpr float RIVER_WIDTH = 0.15f;

WorldFeatureGenerator::WorldFeatureGenerator() : m_seed(12345) {}
void WorldFeatureGenerator::Initialize(unsigned int seed) { m_seed = seed; }

float WorldFeatureGenerator::ContinentalNoise(int x, int z) const {
    // Large-scale deterministic noise (not per-tile noise, continuous)
    float nx = x * CONTINENTAL_FREQUENCY;
    float nz = z * CONTINENTAL_FREQUENCY;
    return std::sin(nx + m_seed*0.01f) * std::cos(nz * 0.7f + m_seed*0.013f) * 0.5f;
}

float WorldFeatureGenerator::LakeDepressionNoise(int x, int z) const {
    float nx = x * LAKE_FREQ;
    float nz = z * LAKE_FREQ;
    return std::sin(nx + 100.0f) * std::cos(nz + 100.0f) * 0.5f;
}

float WorldFeatureGenerator::RiverPathNoise(int x, int z) const {
    float nx = x * RIVER_FREQ;
    float nz = z * RIVER_FREQ;
    return std::sin(nx + 500.0f) * 0.5f + std::cos(nz * 0.5f + 500.0f) * 0.5f;
}

float WorldFeatureGenerator::RiverNarrowNoise(int x, int z) const {
    float nx = x * RIVER_NARROW;
    float nz = z * RIVER_NARROW;
    return std::sin(nx + 500.0f) * 0.5f;
}

WaterFeature WorldFeatureGenerator::GetFeatureAt(int worldX, int worldZ, const TerrainGenerator& tg) const {
    if (IsSea(worldX, worldZ, tg)) return WaterFeature::Sea;
    if (IsLake(worldX, worldZ, tg)) return WaterFeature::Lake;
    if (IsRiver(worldX, worldZ, tg)) return WaterFeature::River;
    return WaterFeature::None;
}

bool WorldFeatureGenerator::IsSea(int worldX, int worldZ, const TerrainGenerator& tg) const {
    int h = tg.GenerateHeightLevel(worldX, worldZ);
    float cont = ContinentalNoise(worldX, worldZ);
    // Sea: low continentalness AND terrain below sea level or naturally low
    return (cont < CONTINENTAL_THRESHOLD) && (h <= (int)SEA_LEVEL);
}

bool WorldFeatureGenerator::IsLake(int worldX, int worldZ, const TerrainGenerator& tg) const {
    int h = tg.GenerateHeightLevel(worldX, worldZ);
    float region = tg.GetRegion(worldX, worldZ);
    if (h > (int)SEA_LEVEL) return false;
    float dep = LakeDepressionNoise(worldX, worldZ);
    return (dep < LAKE_THRESHOLD) && (region != 0);
}

bool WorldFeatureGenerator::IsRiver(int worldX, int worldZ, const TerrainGenerator& tg) const {
    int h = tg.GenerateHeightLevel(worldX, worldZ);
    float path = RiverPathNoise(worldX, worldZ);
    float narrow = RiverNarrowNoise(worldX, worldZ);
    return (path > 0.35f && path < 0.65f) && (std::abs(narrow) < RIVER_WIDTH) && (h <= 3);
}
