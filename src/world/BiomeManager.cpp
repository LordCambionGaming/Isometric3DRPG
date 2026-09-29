#include "world/BiomeManager.h"
#include <raymath.h>
#include <algorithm>
#include <cmath>

BiomeManager::BiomeManager() {
}

void BiomeManager::Initialize() {
    SetupDefaultBiomes();
}

BiomeType BiomeManager::GetBiome(float height, float temperature, float humidity) const {
    float bestScore = -1.0f;
    BiomeType bestBiome = BiomeType::Plains;

    for (const auto& biome : m_biomes) {
        float score = CalculateBiomeScore(biome, height, temperature, humidity);
        if (score > bestScore) {
            bestScore = score;
            bestBiome = biome.type;
        }
    }

    return bestBiome;
}

const BiomeData& BiomeManager::GetBiomeData(BiomeType type) const {
    int index = static_cast<int>(type);
    if (index >= 0 && index < m_biomes.size()) {
        return m_biomes[index];
    }
    return m_biomes[static_cast<int>(BiomeType::Plains)]; // Default fallback
}

Color BiomeManager::GetBiomeColor(BiomeType type, float variation) const {
    const BiomeData& biome = GetBiomeData(type);

    // Blend between base and variant color based on variation
    float blend = Clamp(variation, 0.0f, 1.0f);

    return {
        static_cast<unsigned char>(Lerp(biome.baseColor.r, biome.variantColor.r, blend)),
        static_cast<unsigned char>(Lerp(biome.baseColor.g, biome.variantColor.g, blend)),
        static_cast<unsigned char>(Lerp(biome.baseColor.b, biome.variantColor.b, blend)),
        255
    };
}

Color BiomeManager::BlendBiomeColors(BiomeType biome1, BiomeType biome2, float blend) const {
    const BiomeData& b1 = GetBiomeData(biome1);
    const BiomeData& b2 = GetBiomeData(biome2);

    blend = Clamp(blend, 0.0f, 1.0f);

    return {
        static_cast<unsigned char>(Lerp(b1.baseColor.r, b2.baseColor.r, blend)),
        static_cast<unsigned char>(Lerp(b1.baseColor.g, b2.baseColor.g, blend)),
        static_cast<unsigned char>(Lerp(b1.baseColor.b, b2.baseColor.b, blend)),
        255
    };
}

BiomeType BiomeManager::GetDominantBiome(int worldX, int worldZ, float height, float temperature, float humidity, float* blendFactor) const {
    BiomeType dominant = GetBiome(height, temperature, humidity);
    if (blendFactor) {
        *blendFactor = 1.0f;
    }
    return dominant;
}

void BiomeManager::SetupDefaultBiomes() {
    m_biomes.resize(static_cast<int>(BiomeType::COUNT));

    // Ocean
    m_biomes[static_cast<int>(BiomeType::Ocean)] = {
        BiomeType::Ocean, "Ocean",
        BLUE, DARKBLUE,
        -10.0f, -2.0f, 0.0f, 1.0f, 0.8f, 1.0f,
        0.0f, 0.0f, 0.0f, 0.1f, 0.0f
    };

    // Beach
    m_biomes[static_cast<int>(BiomeType::Beach)] = {
        BiomeType::Beach, "Beach",
        BEIGE, BROWN,
        -2.0f, 0.5f, 0.3f, 0.8f, 0.2f, 0.6f,
        0.1f, 0.05f, 0.2f, 0.1f, 0.01f
    };

    // Plains
    m_biomes[static_cast<int>(BiomeType::Plains)] = {
        BiomeType::Plains, "Plains",
        GREEN, LIME,
        0.0f, 3.0f, 0.2f, 0.8f, 0.3f, 0.7f,
        0.6f, 0.1f, 0.8f, 0.05f, 0.02f
    };

    // Forest
    m_biomes[static_cast<int>(BiomeType::Forest)] = {
        BiomeType::Forest, "Forest",
        DARKGREEN, GREEN,
        1.0f, 5.0f, 0.4f, 0.9f, 0.6f, 1.0f,
        0.9f, 0.7f, 0.3f, 0.2f, 0.01f
    };

    // Hills
    m_biomes[static_cast<int>(BiomeType::Hills)] = {
        BiomeType::Hills, "Hills",
        BROWN, DARKBROWN,
        3.0f, 6.0f, 0.1f, 0.6f, 0.2f, 0.6f,
        0.4f, 0.3f, 0.2f, 0.3f, 0.03f
    };

    // Mountains
    m_biomes[static_cast<int>(BiomeType::Mountains)] = {
        BiomeType::Mountains, "Mountains",
        GRAY, LIGHTGRAY,
        5.0f, 15.0f, 0.0f, 0.3f, 0.1f, 0.4f,
        0.1f, 0.05f, 0.1f, 0.4f, 0.05f
    };

    // Desert
    m_biomes[static_cast<int>(BiomeType::Desert)] = {
        BiomeType::Desert, "Desert",
        YELLOW, GOLD,
        0.5f, 3.0f, 0.6f, 1.0f, 0.0f, 0.2f,
        0.05f, 0.01f, 0.05f, 0.1f, 0.02f
    };

    // Swamp
    m_biomes[static_cast<int>(BiomeType::Swamp)] = {
        BiomeType::Swamp, "Swamp",
        DARKGREEN, BROWN,
        -1.0f, 1.0f, 0.3f, 0.7f, 0.8f, 1.0f,
        0.8f, 0.4f, 0.9f, 0.1f, 0.01f
    };
}

float BiomeManager::CalculateBiomeScore(const BiomeData& biome, float height, float temperature, float humidity) const {
    float score = 0.0f;

    // Height score
    if (height >= biome.heightMin && height <= biome.heightMax) {
        score += 1.0f;
    } else {
        float heightDist = std::min(
            std::abs(height - biome.heightMin),
            std::abs(height - biome.heightMax)
        );
        score += std::max(0.0f, 1.0f - heightDist * 0.2f);
    }

    // Temperature score
    if (temperature >= biome.temperatureMin && temperature <= biome.temperatureMax) {
        score += 1.0f;
    } else {
        float tempDist = std::min(
            std::abs(temperature - biome.temperatureMin),
            std::abs(temperature - biome.temperatureMax)
        );
        score += std::max(0.0f, 1.0f - tempDist * 2.0f);
    }

    // Humidity score
    if (humidity >= biome.humidityMin && humidity <= biome.humidityMax) {
        score += 1.0f;
    } else {
        float humidityDist = std::min(
            std::abs(humidity - biome.humidityMin),
            std::abs(humidity - biome.humidityMax)
        );
        score += std::max(0.0f, 1.0f - humidityDist * 2.0f);
    }

    return score / 3.0f; // Normalize to 0-1 range
}