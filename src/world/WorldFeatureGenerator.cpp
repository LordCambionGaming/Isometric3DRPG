#include "world/WorldFeatureGenerator.h"
#include "world/TerrainGenerator.h"
#include <cmath>

static constexpr float CONTINENTAL_FREQUENCY = 0.003f;
static constexpr float CONTINENTAL_THRESHOLD = -0.28f;
static constexpr float SEA_LEVEL = 2.0f;
static constexpr float LAKE_FREQ = 0.03f;
static constexpr float LAKE_THRESHOLD = -0.20f;
static constexpr float RIVER_FREQ = 0.008f;
static constexpr float RIVER_WIDTH = 0.15f;

WorldFeatureGenerator::WorldFeatureGenerator() : m_seed(12345) {}
void WorldFeatureGenerator::Initialize(unsigned int seed) { m_seed = seed; }

float WorldFeatureGenerator::ContinentalNoise(int x, int z) const {
    float nx = x * CONTINENTAL_FREQUENCY;
    float nz = z * CONTINENTAL_FREQUENCY;
    return std::sin(nx + m_seed*0.01f) * std::cos(nz * 0.7f + m_seed*0.013f) * 0.5f + std::sin(nz * 0.5f + m_seed*0.007f) * 0.25f;
}
float WorldFeatureGenerator::LakeDepressionNoise(int x, int z) const {
    float nx = x * LAKE_FREQ; float nz = z * LAKE_FREQ;
    return std::sin(nx + 100.0f) * std::cos(nz + 100.0f) * 0.5f;
}
float WorldFeatureGenerator::RiverPathNoise(int x, int z) const {
    float nx = x * RIVER_FREQ; float nz = z * RIVER_FREQ;
    float n1 = std::sin(nx * 1.0f + 500.0f) * std::cos(nz * 0.8f + 200.0f);
    float n2 = std::sin(nx * 0.5f + nz * 0.5f + m_seed*0.001f) * 0.5f;
    return n1 * 0.5f + n2 * 0.5f + 0.5f;
}
float WorldFeatureGenerator::RiverNarrowNoise(int x, int z) const {
    float nx = x * 0.06f; float nz = z * 0.06f;
    return std::sin(nx + nz + 500.0f) * 0.5f + std::sin(nx*1.7f - nz*1.3f + 100.0f) * 0.25f;
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
    if (h > 3) return false;
    float path = RiverPathNoise(worldX, worldZ);
    float distFromCenter = std::abs(path - 0.5f);
    float narrow = RiverNarrowNoise(worldX, worldZ);
    bool inRiverBand = distFromCenter < 0.08f; // più stretto di prima
    bool inNarrow = std::abs(narrow) < RIVER_WIDTH;
    float breakNoise = std::sin(worldZ * 0.02f + worldX * 0.01f) * 0.5f + 0.5f;
    if (breakNoise < 0.15f) return false; // guadi
    return inRiverBand && inNarrow;
}