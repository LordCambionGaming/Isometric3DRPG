#pragma once

#include <raylib.h>
#include <vector>
#include <string>

enum class BiomeType {
    Ocean = 0,
    Beach,
    Plains,
    Forest,
    Hills,
    Mountains,
    Desert,
    Swamp,
    COUNT
};

struct BiomeData {
    BiomeType type;
    std::string name;
    Color baseColor;
    Color variantColor;
    float heightMin;
    float heightMax;
    float temperatureMin;
    float temperatureMax;
    float humidityMin;
    float humidityMax;

    // Vegetation parameters
    float vegetationDensity;
    float treeChance;
    float grassChance;
    float rockChance;

    // Structure parameters
    float structureChance;
};

class BiomeManager {
public:
    BiomeManager();
    ~BiomeManager() = default;

    void Initialize();

    BiomeType GetBiome(float height, float temperature, float humidity) const;
    const BiomeData& GetBiomeData(BiomeType type) const;

    Color GetBiomeColor(BiomeType type, float variation) const;
    Color BlendBiomeColors(BiomeType biome1, BiomeType biome2, float blend) const;

    // Biome transition — now receives real terrain climate data
    BiomeType GetDominantBiome(int worldX, int worldZ, float height, float temperature, float humidity, float* blendFactor = nullptr) const;

    const std::vector<BiomeData>& GetAllBiomes() const { return m_biomes; }

private:
    void SetupDefaultBiomes();
    float CalculateBiomeScore(const BiomeData& biome, float height, float temperature, float humidity) const;

    std::vector<BiomeData> m_biomes;
};