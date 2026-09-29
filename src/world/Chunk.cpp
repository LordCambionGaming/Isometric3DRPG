#include "world/Chunk.h"
#include "world/TerrainGenerator.h"
#include <raylib.h>
#include <raymath.h>
#include <vector>
#include <cmath>
#include <iostream>

Chunk::Chunk(int x, int z, int size, float tileSize)
    : m_x(x)
    , m_z(z)
    , m_size(size)
    , m_tileSize(tileSize)
    , m_generated(false)
    , m_hasMesh(false)
{
    m_tiles.resize(size * size);
}

Chunk::~Chunk() {
    Unload();
}

void Chunk::Generate(unsigned int worldSeed) {
    if (m_generated) return;

    GenerateTerrain(worldSeed);
    GenerateColors();

    m_generated = true;
}

void Chunk::BuildMesh() {
    if (!m_generated || m_hasMesh) return;

    CreateTerrainMesh();
    m_hasMesh = true;
}

void Chunk::Render() {
    if (!m_hasMesh) return;

    Vector3 worldPos = GetWorldPosition();
    DrawModel(m_model, worldPos, 1.0f, WHITE);
}

void Chunk::Unload() {
    if (m_hasMesh) {
        UnloadModel(m_model);
        m_hasMesh = false;
    }
    m_generated = false;
}

Vector3 Chunk::GetWorldPosition() const {
    return {
        m_x * m_size * m_tileSize,
        0.0f,
        m_z * m_size * m_tileSize
    };
}

float Chunk::GetHeightAt(float localX, float localZ) const {
    if (!m_generated) return 0.0f;

    // Clamp coordinates to chunk bounds
    int tileX = static_cast<int>(localX / m_tileSize);
    int tileZ = static_cast<int>(localZ / m_tileSize);

    if (tileX < 0 || tileX >= m_size || tileZ < 0 || tileZ >= m_size) {
        return 0.0f;
    }

    return GetTile(tileX, tileZ).height;
}

void Chunk::GenerateTerrain(unsigned int seed) {
    // Enhanced procedural terrain generation using improved TerrainGenerator
    for (int z = 0; z < m_size; z++) {
        for (int x = 0; x < m_size; x++) {
            // World coordinates
            int worldX = m_x * m_size + x;
            int worldZ = m_z * m_size + z;

            TerrainTile& tile = GetTile(x, z);

            // Generate height using enhanced noise
            float height = 0.0f;
            float frequency = 0.015f;
            float amplitude = 12.0f;

            // Multiple octaves for realistic terrain
            for (int octave = 0; octave < 5; octave++) {
                float octaveFreq = frequency * (1 << octave);
                float octaveAmp = amplitude / (1 << octave);

                // Enhanced hash-based noise
                unsigned int hash = seed + octave * 1000;
                hash ^= (worldX) * 374761393U;
                hash ^= (worldZ) * 668265263U;
                hash = hash * 1664525U + 1013904223U;

                float noise = (hash / (float)0xFFFFFFFFU) * 2.0f - 1.0f;
                height += noise * octaveAmp * octaveFreq;
            }

            tile.height = height;

            // Generate temperature (decreases with latitude and altitude)
            float latitudeEffect = 0.8f - (std::abs(worldZ) * 0.0001f);
            unsigned int tempHash = seed + 2000;
            tempHash ^= worldX * 668265263U;
            tempHash ^= worldZ * 374761393U;
            tempHash = tempHash * 1664525U + 1013904223U;
            float tempNoise = (tempHash / (float)0xFFFFFFFFU) * 0.4f - 0.2f;
            tile.temperature = Clamp(latitudeEffect + tempNoise - (height * 0.05f), 0.0f, 1.0f);

            // Generate humidity
            unsigned int humidHash = seed + 3000;
            humidHash ^= worldX * 1013904223U;
            humidHash ^= worldZ * 1664525U;
            humidHash = humidHash * 374761393U + 668265263U;
            float humidNoise = (humidHash / (float)0xFFFFFFFFU) * 0.6f - 0.3f;
            tile.humidity = Clamp(0.5f + humidNoise, 0.0f, 1.0f);

            // Biome from BiomeManager via World pipeline (store only — Chunk does NOT decide biome)
            tile.biomeType = 0; // to be overwritten by World with real BiomeManager result

            // Set vegetation flag based on biome and randomness
            unsigned int vegHash = seed + 4000;
            vegHash ^= (worldX + worldZ) * 1013904223U;
            vegHash = vegHash * 1664525U + 374761393U;
            float vegChance = (vegHash / (float)0xFFFFFFFFU);
            tile.hasVegetation = ShouldHaveVegetation(tile.biomeType, vegChance);
        }
    }
}

void Chunk::GenerateColors() {
    for (int z = 0; z < m_size; z++) {
        for (int x = 0; x < m_size; x++) {
            TerrainTile& tile = GetTile(x, z);
            GenerateTileColors(tile);
        }
    }
}

bool Chunk::ShouldHaveVegetation(int biomeType, float randomValue) const {
    switch (biomeType) {
        case 0: return false; // Ocean
        case 1: return randomValue < 0.1f; // Beach - sparse vegetation
        case 2: return randomValue < 0.6f; // Plains - moderate vegetation
        case 3: return randomValue < 0.9f; // Forest - dense vegetation
        case 4: return randomValue < 0.4f; // Hills - sparse vegetation
        case 5: return randomValue < 0.2f; // Mountains - very sparse
        case 6: return randomValue < 0.05f; // Desert - very rare
        case 7: return randomValue < 0.8f; // Swamp - dense vegetation
        default: return randomValue < 0.3f;
    }
}

void Chunk::GenerateTileColors(TerrainTile& tile) const {
    // Enhanced color generation based on biome, height, and environmental factors
    switch (tile.biomeType) {
        case 0: // Ocean
            tile.baseColor = (tile.height < -4.0f) ? DARKBLUE : BLUE;
            tile.detailColor = SKYBLUE;
            tile.roughness = 0.1f;
            break;

        case 1: // Beach
            tile.baseColor = BEIGE;
            tile.detailColor = BROWN;
            tile.roughness = 0.8f;
            break;

        case 2: // Plains
            {
                float grassVariation = (tile.temperature + tile.humidity) * 0.5f;
                if (grassVariation > 0.7f) {
                    tile.baseColor = LIME;
                } else if (grassVariation > 0.4f) {
                    tile.baseColor = GREEN;
                } else {
                    tile.baseColor = DARKGREEN;
                }
                tile.detailColor = tile.hasVegetation ? LIME : tile.baseColor;
                tile.roughness = 0.6f;
            }
            break;

        case 3: // Forest
            tile.baseColor = DARKGREEN;
            tile.detailColor = tile.hasVegetation ? GREEN : BROWN;
            tile.roughness = 0.9f;
            break;

        case 4: // Hills
            {
                float rockiness = 1.0f - tile.humidity;
                if (rockiness > 0.7f) {
                    tile.baseColor = GRAY;
                    tile.detailColor = LIGHTGRAY;
                } else {
                    tile.baseColor = BROWN;
                    tile.detailColor = tile.hasVegetation ? GREEN : DARKBROWN;
                }
                tile.roughness = 0.8f;
            }
            break;

        case 5: // Mountains
            tile.baseColor = GRAY;
            tile.detailColor = tile.height > 10.0f ? WHITE : LIGHTGRAY; // Snow cap
            tile.roughness = 0.9f;
            break;

        case 6: // Desert
            {
                float sandVariation = tile.temperature;
                if (sandVariation > 0.8f) {
                    tile.baseColor = GOLD;
                } else {
                    tile.baseColor = YELLOW;
                }
                tile.detailColor = tile.hasVegetation ? GREEN : tile.baseColor;
                tile.roughness = 0.7f;
            }
            break;

        case 7: // Swamp
            tile.baseColor = DARKGREEN;
            tile.detailColor = tile.hasVegetation ? GREEN : BROWN;
            tile.roughness = 0.4f; // Wet surface
            break;

        default:
            tile.baseColor = GREEN;
            tile.detailColor = GREEN;
            tile.roughness = 0.5f;
            break;
    }
}

void Chunk::CreateTerrainMesh() {
    std::vector<Vector3> vertices;
    std::vector<Vector2> texCoords;
    std::vector<Vector3> normals;
    std::vector<Color> colors;

    // Generate enhanced mesh vertices with proper normals
    for (int z = 0; z < m_size; z++) {
        for (int x = 0; x < m_size; x++) {
            const TerrainTile& tile = GetTile(x, z);

            // Create vertices for this tile
            Vector3 v1 = { x * m_tileSize, tile.height, z * m_tileSize };
            Vector3 v2 = { (x + 1) * m_tileSize, tile.height, z * m_tileSize };
            Vector3 v3 = { (x + 1) * m_tileSize, tile.height, (z + 1) * m_tileSize };
            Vector3 v4 = { x * m_tileSize, tile.height, (z + 1) * m_tileSize };

            // Get neighboring heights for proper normal calculation
            float heightRight = (x + 1 < m_size) ? GetTile(x + 1, z).height : tile.height;
            float heightDown = (z + 1 < m_size) ? GetTile(x, z + 1).height : tile.height;

            // Adjust corner heights for smoother transitions
            if (x + 1 < m_size) v2.y = heightRight;
            if (z + 1 < m_size) v4.y = heightDown;
            if (x + 1 < m_size && z + 1 < m_size) v3.y = GetTile(x + 1, z + 1).height;

            // Choose color based on tile properties
            Color tileColor = BlendColors(tile.baseColor, tile.detailColor, tile.hasVegetation ? 0.3f : 0.0f);

            // Add the quad (two triangles) with enhanced colors
            AddEnhancedQuad(vertices, texCoords, normals, colors, v1, v2, v3, v4, tileColor, tile);
        }
    }

    // Create the mesh
    m_mesh = { 0 };
    m_mesh.vertexCount = vertices.size();
    m_mesh.triangleCount = vertices.size() / 3;

    // Allocate mesh arrays
    m_mesh.vertices = (float*)MemAlloc(vertices.size() * 3 * sizeof(float));
    m_mesh.texcoords = (float*)MemAlloc(texCoords.size() * 2 * sizeof(float));
    m_mesh.normals = (float*)MemAlloc(normals.size() * 3 * sizeof(float));
    m_mesh.colors = (unsigned char*)MemAlloc(colors.size() * 4 * sizeof(unsigned char));

    // Copy data to mesh
    for (size_t i = 0; i < vertices.size(); i++) {
        m_mesh.vertices[i * 3 + 0] = vertices[i].x;
        m_mesh.vertices[i * 3 + 1] = vertices[i].y;
        m_mesh.vertices[i * 3 + 2] = vertices[i].z;

        m_mesh.normals[i * 3 + 0] = normals[i].x;
        m_mesh.normals[i * 3 + 1] = normals[i].y;
        m_mesh.normals[i * 3 + 2] = normals[i].z;

        m_mesh.colors[i * 4 + 0] = colors[i].r;
        m_mesh.colors[i * 4 + 1] = colors[i].g;
        m_mesh.colors[i * 4 + 2] = colors[i].b;
        m_mesh.colors[i * 4 + 3] = colors[i].a;
    }

    for (size_t i = 0; i < texCoords.size(); i++) {
        m_mesh.texcoords[i * 2 + 0] = texCoords[i].x;
        m_mesh.texcoords[i * 2 + 1] = texCoords[i].y;
    }

    // Upload mesh to GPU
    UploadMesh(&m_mesh, false);

    // Create model from mesh
    m_model = LoadModelFromMesh(m_mesh);
}

Color Chunk::BlendColors(Color color1, Color color2, float factor) const {
    factor = Clamp(factor, 0.0f, 1.0f);
    return {
        (unsigned char)(color1.r + (color2.r - color1.r) * factor),
        (unsigned char)(color1.g + (color2.g - color1.g) * factor),
        (unsigned char)(color1.b + (color2.b - color1.b) * factor),
        255
    };
}

void Chunk::AddEnhancedQuad(std::vector<Vector3>& vertices, std::vector<Vector2>& texCoords,
                           std::vector<Vector3>& normals, std::vector<Color>& colors,
                           Vector3 v1, Vector3 v2, Vector3 v3, Vector3 v4, Color color, const TerrainTile& tile) {
    // Calculate proper normal for the quad
    Vector3 edge1 = Vector3Subtract(v2, v1);
    Vector3 edge2 = Vector3Subtract(v4, v1);
    Vector3 normal = Vector3Normalize(Vector3CrossProduct(edge1, edge2));

    // Add slight color variation based on tile properties
    Color finalColor = color;
    if (tile.roughness > 0.7f) {
        // Darken rough surfaces slightly
        finalColor.r = (unsigned char)(finalColor.r * 0.9f);
        finalColor.g = (unsigned char)(finalColor.g * 0.9f);
        finalColor.b = (unsigned char)(finalColor.b * 0.9f);
    }

    // First triangle (v1, v2, v3)
    vertices.push_back(v1);
    vertices.push_back(v2);
    vertices.push_back(v3);

    texCoords.push_back({ 0.0f, 0.0f });
    texCoords.push_back({ 1.0f, 0.0f });
    texCoords.push_back({ 1.0f, 1.0f });

    normals.push_back(normal);
    normals.push_back(normal);
    normals.push_back(normal);

    colors.push_back(finalColor);
    colors.push_back(finalColor);
    colors.push_back(finalColor);

    // Second triangle (v1, v3, v4)
    vertices.push_back(v1);
    vertices.push_back(v3);
    vertices.push_back(v4);

    texCoords.push_back({ 0.0f, 0.0f });
    texCoords.push_back({ 1.0f, 1.0f });
    texCoords.push_back({ 0.0f, 1.0f });

    normals.push_back(normal);
    normals.push_back(normal);
    normals.push_back(normal);

    colors.push_back(finalColor);
    colors.push_back(finalColor);
    colors.push_back(finalColor);
}

void Chunk::AddQuad(std::vector<Vector3>& vertices, std::vector<Vector2>& texCoords,
                    std::vector<Vector3>& normals, std::vector<Color>& colors,
                    Vector3 v1, Vector3 v2, Vector3 v3, Vector3 v4, Color color) {
    // Calculate normal (assuming flat quad for now)
    Vector3 normal = { 0.0f, 1.0f, 0.0f };

    // First triangle (v1, v2, v3)
    vertices.push_back(v1);
    vertices.push_back(v2);
    vertices.push_back(v3);

    texCoords.push_back({ 0.0f, 0.0f });
    texCoords.push_back({ 1.0f, 0.0f });
    texCoords.push_back({ 1.0f, 1.0f });

    normals.push_back(normal);
    normals.push_back(normal);
    normals.push_back(normal);

    colors.push_back(color);
    colors.push_back(color);
    colors.push_back(color);

    // Second triangle (v1, v3, v4)
    vertices.push_back(v1);
    vertices.push_back(v3);
    vertices.push_back(v4);

    texCoords.push_back({ 0.0f, 0.0f });
    texCoords.push_back({ 1.0f, 1.0f });
    texCoords.push_back({ 0.0f, 1.0f });

    normals.push_back(normal);
    normals.push_back(normal);
    normals.push_back(normal);

    colors.push_back(color);
    colors.push_back(color);
    colors.push_back(color);
}