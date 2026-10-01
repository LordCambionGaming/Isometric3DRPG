#include "world/Chunk.h"
#include "world/TerrainGenerator.h"
#include "world/World.h"
#include <cmath>

// Local helper for water noise (TerrainGenerator::PerlinNoise is private)
#include <raylib.h>
#include <raymath.h>
#include <vector>
#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>

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

void Chunk::Generate(const TerrainGenerator& terrainGenerator, World* world) {
    if (m_generated) return;
    GenerateTerrain(terrainGenerator, world);
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
    float s = m_tileSize;

    // Draw voxel mesh (solid block columns per tile, with face culling)
    DrawModel(m_model, GetWorldPosition(), 1.0f, WHITE);

    // D&D 16x16 grid: 17 vertical/horizontal lines at each tile top surface
    // Use actual tile heights from terrain generation so grid aligns with block tops
    for (int i = 0; i <= m_size; i++) {
        float xPos = m_x * m_size * s + i * s;
        for (int z = 0; z <= m_size; z++) {
            float zPos = m_z * m_size * s + z * s;
            int tx = (i < m_size) ? i : m_size - 1;
            int tz = (z < m_size) ? z : m_size - 1;
            if (tx < 0) tx = 0; if (tz < 0) tz = 0;
            int hh = static_cast<int>(GetTile(tx, tz).height);
            if (hh < 1) hh = 1;
            float yGrid = hh * s + 0.02f;
            float zStart = zPos;
            float zEnd = (z < m_size) ? zPos + s : zPos;
            DrawLine3D({xPos, yGrid, zStart}, {xPos, yGrid, zEnd}, DARKGRAY);
        }
    }
    for (int j = 0; j <= m_size; j++) {
        float zPos = m_z * m_size * s + j * s;
        for (int x = 0; x <= m_size; x++) {
            float xPos = m_x * m_size * s + x * s;
            int tx = (x < m_size) ? x : m_size - 1;
            int tz = (j < m_size) ? j : m_size - 1;
            if (tx < 0) tx = 0; if (tz < 0) tz = 0;
            int hh = static_cast<int>(GetTile(tx, tz).height);
            if (hh < 1) hh = 1;
            float yGrid = hh * s + 0.02f;
            float xStart = xPos;
            float xEnd = (x < m_size) ? xPos + s : xPos;
            DrawLine3D({xStart, yGrid, zPos}, {xEnd, yGrid, zPos}, DARKGRAY);
        }
    }
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
    int tileX = static_cast<int>(localX / m_tileSize);
    int tileZ = static_cast<int>(localZ / m_tileSize);
    if (tileX < 0 || tileX >= m_size || tileZ < 0 || tileZ >= m_size) return 0.0f;
    return GetTile(tileX, tileZ).height;
}

void Chunk::GenerateTerrain(const TerrainGenerator& terrainGenerator, World* world) {
    auto tStart = std::chrono::high_resolution_clock::now();
    const int chunkOffsetX = m_x * m_size;
    const int chunkOffsetZ = m_z * m_size;
    const int waterLevel = 1;
    int tileWaterCount = 0;
    int minHeight = 999, maxHeight = -1;

    auto tHeightStart = std::chrono::high_resolution_clock::now();
    for (int z = 0; z < m_size; z++) {
        for (int x = 0; x < m_size; x++) {
            const int index = z * m_size + x;
            TerrainTile& tile = m_tiles[index];
            const int worldX = chunkOffsetX + x;
            const int worldZ = chunkOffsetZ + z;

            int level = terrainGenerator.GenerateHeightLevel(worldX, worldZ);
            tile.height = static_cast<float>(level);
            if (level < minHeight) minHeight = level;
            if (level > maxHeight) maxHeight = level;

            tile.temperature = terrainGenerator.GenerateTemperature(worldX, worldZ);
            tile.humidity = terrainGenerator.GenerateHumidity(worldX, worldZ);
            tile.biomeType = 0;
            tile.hasVegetation = false;
        }
    }
    auto tHeightEnd = std::chrono::high_resolution_clock::now();

    auto tWaterStart = std::chrono::high_resolution_clock::now();
    for (int z = 0; z < m_size; z++) {
        for (int x = 0; x < m_size; x++) {
            const int index = z * m_size + x;
            TerrainTile& tile = m_tiles[index];
            const int worldX = chunkOffsetX + x;
            const int worldZ = chunkOffsetZ + z;
            int level = static_cast<int>(tile.height);

            int region = terrainGenerator.GetRegion(worldX, worldZ);
            float riverShape = terrainGenerator.GenerateHeight(worldX * 0.008f, worldZ * 0.008f);
            float riverNarrow = terrainGenerator.GenerateHeight(worldX * 0.06f + 500.0f, worldZ * 0.06f + 500.0f);
            bool isRiver = (riverShape > 0.35f && riverShape < 0.65f) && (std::abs(riverNarrow) < 0.15f) && (level <= 3);
            float depression = terrainGenerator.GenerateHeight(worldX * 0.03f + 100.0f, worldZ * 0.03f + 100.0f);
            bool isLake = (level == 1) && (region == 0 || region == 4) && (depression < 0.25f);
            bool isWaterCandidate = isRiver || isLake;

            // Regola confinamento acqua: solo se tutti i 4 vicini cardinali sono solidi
            // Se anche UNO è vuoto (fuori chunk o vuoto nel chunk) → NO ACQUA
            // Conservativo ai bordi chunk: se non posso verificare vicino esterno → niente acqua
            int nHeight = 0, sHeight = 0, eHeight = 0, wHeight = 0;

            // Calcolo deterministico per coord mondo: non dipende dall'ordine di generazione chunk
            // worldX/worldZ già definiti nel loop esterno (riga 144-145)

            // North (worldZ - 1) — coerenza geometrica, non solo >0
            nHeight = (z > 0) ? static_cast<int>(m_tiles[(z-1)*m_size + x].height) : terrainGenerator.GenerateHeightLevel(worldX, worldZ - 1);
            bool northSolid = (nHeight > 0) && (std::abs(nHeight - level) <= 1);
            // South (worldZ + 1)
            sHeight = (z < m_size - 1) ? static_cast<int>(m_tiles[(z+1)*m_size + x].height) : terrainGenerator.GenerateHeightLevel(worldX, worldZ + 1);
            bool southSolid = (sHeight > 0) && (std::abs(sHeight - level) <= 1);
            // East (worldX + 1)
            eHeight = (x < m_size - 1) ? static_cast<int>(m_tiles[z*m_size + (x+1)].height) : terrainGenerator.GenerateHeightLevel(worldX + 1, worldZ);
            bool eastSolid = (eHeight > 0) && (std::abs(eHeight - level) <= 1);
            // West (worldX - 1)
            wHeight = (x > 0) ? static_cast<int>(m_tiles[z*m_size + (x-1)].height) : terrainGenerator.GenerateHeightLevel(worldX - 1, worldZ);
            bool westSolid = (wHeight > 0) && (std::abs(wHeight - level) <= 1);
            bool allNeighborsSolid = northSolid && southSolid && eastSolid && westSolid;
            bool isContained = isWaterCandidate && allNeighborsSolid;

            // Log temporaneo caso problematico (acqua volante / dislivello)
            static int logCheck = 0;
            if (logCheck < 8 && isWaterCandidate && (z==0||x==0||z==m_size-1||x==m_size-1)) {
                std::cout << "WATER CHECK world=(" << worldX << "," << worldZ << ") height=" << level
                          << " waterH=" << waterLevel
                          << " N=" << nHeight << " S=" << sHeight << " E=" << eHeight << " W=" << wHeight
                          << " result=" << (isContained ? "WATER" : "NONE") << std::endl;
                logCheck++;
            }

            // Fase 3: eliminare buchi interni (piccoli gruppi asciutti dentro formazione acqua)
            // NON riempire bordi: solo buchi completamente circondati da acqua della stessa formazione
            // Per questa versione: mantenere conservativo. Se tile non contenuto, non acqua.
            bool isWater = isContained;

            tile.hasWater = isWater;
            tile.waterHeight = static_cast<float>(waterLevel);
            if (isWater) {
                if (isRiver) tile.waterFeature = 2;
                else if (isLake) tile.waterFeature = 1;
                else tile.waterFeature = 0;
                tileWaterCount++;
            } else {
                tile.waterFeature = 0;
            }
        }
    }
    auto tWaterEnd = std::chrono::high_resolution_clock::now();
    auto tEnd = std::chrono::high_resolution_clock::now();

    long heightMs = std::chrono::duration_cast<std::chrono::milliseconds>(tHeightEnd - tHeightStart).count();
    long waterMs = std::chrono::duration_cast<std::chrono::milliseconds>(tWaterEnd - tWaterStart).count();
    long totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart).count();

    if (minHeight < 1) minHeight = 1;
    int dryTiles = (m_size * m_size) - tileWaterCount;
    std::cout << "[GEN PERF] Chunk (" << m_x << "," << m_z << ")" << std::endl;
    std::cout << "  Height+Biome: " << heightMs << " ms" << std::endl;
    std::cout << "  Water: " << waterMs << " ms" << std::endl;
    std::cout << "  Generate total: " << totalMs << " ms" << std::endl;
    std::cout << "  dry=" << dryTiles << " lake+river=" << tileWaterCount
              << " minH=" << minHeight << " maxH=" << maxHeight << std::endl;
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
        case 0: return false;
        case 1: return randomValue < 0.1f;
        case 2: return randomValue < 0.6f;
        case 3: return randomValue < 0.9f;
        case 4: return randomValue < 0.4f;
        case 5: return randomValue < 0.2f;
        case 6: return randomValue < 0.05f;
        case 7: return randomValue < 0.8f;
        default: return randomValue < 0.3f;
    }
}

void Chunk::GenerateTileColors(TerrainTile& tile) const {
    switch (tile.biomeType) {
        case 0: tile.baseColor = DARKBLUE; tile.detailColor = BLUE; tile.roughness = 0.1f; break;
        case 1: tile.baseColor = BEIGE; tile.detailColor = BROWN; tile.roughness = 0.8f; break;
        case 2: tile.baseColor = GREEN; tile.detailColor = LIME; tile.roughness = 0.6f; break;
        case 3: tile.baseColor = DARKGREEN; tile.detailColor = GREEN; tile.roughness = 0.9f; break;
        case 4: tile.baseColor = BROWN; tile.detailColor = DARKBROWN; tile.roughness = 0.8f; break;
        case 5: tile.baseColor = GRAY; tile.detailColor = LIGHTGRAY; tile.roughness = 0.9f; break;
        case 6: tile.baseColor = GOLD; tile.detailColor = YELLOW; tile.roughness = 0.7f; break;
        case 7: tile.baseColor = DARKGREEN; tile.detailColor = BROWN; tile.roughness = 0.4f; break;
        default: tile.baseColor = GREEN; tile.detailColor = GREEN; tile.roughness = 0.5f;
    }
}

void Chunk::CreateTerrainMesh() {
    auto tMeshStart = std::chrono::high_resolution_clock::now();

    std::vector<Vector3> verts;
    std::vector<Vector2> texs;
    std::vector<Vector3> norms;
    std::vector<Color> cols;

    // Reserve: estimate for 16x16 chunk, avg height 3, ~50% exposed faces
    // Each quad = 6 vertices (2 tris), estimate ~6k-8k vertices
    const int estimatedVerts = m_size * m_size * 3 * 3 * 6;
    verts.reserve(estimatedVerts);
    texs.reserve(estimatedVerts);
    norms.reserve(estimatedVerts);
    cols.reserve(estimatedVerts);

    float s = m_tileSize;
    for (int z = 0; z < m_size; ++z) {
        for (int x = 0; x < m_size; ++x) {
            int level = static_cast<int>(GetTile(x, z).height);
            if (level < 1) level = 1;
            float baseY = 0.0f;
            float minX = x * s; float maxX = (x + 1) * s;
            float minZ = z * s; float maxZ = (z + 1) * s;
            int neighborLeft = (x > 0) ? static_cast<int>(GetTile(x-1, z).height) : level;
            int neighborRight = (x < m_size - 1) ? static_cast<int>(GetTile(x+1, z).height) : level;
            int neighborBack = (z > 0) ? static_cast<int>(GetTile(x, z-1).height) : level;
            int neighborFront = (z < m_size - 1) ? static_cast<int>(GetTile(x, z+1).height) : level;
            Color topColor = { 34, 139, 34, 255 };
            Color bodyColor = { 160, 120, 60, 255 };
            Color faceColor = bodyColor;
            Color waterColor = { 0, 120, 200, 180 };
            bool hasWater = GetTile(x, z).hasWater;

            // Water surface (only if tile has water)
            if (hasWater) {
                const int waterLevel = 1;
                float yWater = static_cast<float>(waterLevel) * s;
                AddQuadTilted(verts, texs, norms, cols,
                    {minX, yWater, minZ}, {minX, yWater, maxZ}, {maxX, yWater, maxZ}, {maxX, yWater, minZ},
                    Color{0, 120, 200, 255}, Vector3{0,1,0});
            }

            // Water base plane (always drawn at y=0)
            AddQuadTilted(verts, texs, norms, cols,
                {minX, 0.0f, minZ}, {minX, 0.0f, maxZ}, {maxX, 0.0f, maxZ}, {maxX, 0.0f, minZ},
                waterColor, Vector3{0,-1,0});

            for (int y = 0; y < level; ++y) {
                float yBase = baseY + y * s;
                float yTop  = yBase + s;
                Color sideColor = (y == level - 1) ? topColor : faceColor;

                // Top face — sempre generata (visibilità corretta senza culling aggressivo)
                {
                    Vector3 n = { 0.0f, 1.0f, 0.0f };
                    AddQuadTilted(verts, texs, norms, cols,
                        {minX, yTop, minZ}, {minX, yTop, maxZ}, {maxX, yTop, maxZ}, {maxX, yTop, minZ},
                        topColor, n);
                }
                int drawLeft = (neighborLeft != level || y == 0 || y == level - 1) ? 1 : 0;
                if (drawLeft) {
                    AddQuad(verts, texs, norms, cols,
                        {minX, yBase, minZ}, {minX, yBase, maxZ}, {minX, yTop, maxZ}, {minX, yTop, minZ},
                        sideColor, Vector3{-1,0,0});
                }
                int drawRight = (neighborRight != level || y == 0 || y == level - 1) ? 1 : 0;
                if (drawRight) {
                    AddQuad(verts, texs, norms, cols,
                        {maxX, yBase, minZ}, {maxX, yTop, minZ}, {maxX, yTop, maxZ}, {maxX, yBase, maxZ},
                        sideColor, Vector3{1,0,0});
                }
                int drawBack = (neighborBack != level || y == 0 || y == level - 1) ? 1 : 0;
                if (drawBack) {
                    AddQuad(verts, texs, norms, cols,
                        {minX, yBase, maxZ}, {maxX, yBase, maxZ}, {maxX, yTop, maxZ}, {minX, yTop, maxZ},
                        sideColor, Vector3{0,0,1});
                }
                int drawFront = (neighborFront != level || y == 0 || y == level - 1) ? 1 : 0;
                if (drawFront) {
                    AddQuad(verts, texs, norms, cols,
                        {minX, yBase, minZ}, {minX, yTop, minZ}, {maxX, yTop, minZ}, {maxX, yBase, minZ},
                        sideColor, Vector3{0,0,-1});
                }
            }
        }
    }
    auto tMeshEnd = std::chrono::high_resolution_clock::now();

    m_mesh = { 0 };
    int vertexCount = static_cast<int>(verts.size());
    int triangleCount = vertexCount / 3;
    m_mesh.vertexCount = vertexCount;
    m_mesh.triangleCount = triangleCount;
    m_mesh.vertices = (float*)MemAlloc(vertexCount * 3 * sizeof(float));
    m_mesh.texcoords = (float*)MemAlloc(vertexCount * 2 * sizeof(float));
    m_mesh.normals = (float*)MemAlloc(vertexCount * 3 * sizeof(float));
    m_mesh.colors = (unsigned char*)MemAlloc(vertexCount * 4 * sizeof(unsigned char));
    for (int i = 0; i < vertexCount; ++i) {
        m_mesh.vertices[i*3+0] = verts[i].x;
        m_mesh.vertices[i*3+1] = verts[i].y;
        m_mesh.vertices[i*3+2] = verts[i].z;
        m_mesh.texcoords[i*2+0] = texs[i].x;
        m_mesh.texcoords[i*2+1] = texs[i].y;
        m_mesh.normals[i*3+0] = norms[i].x;
        m_mesh.normals[i*3+1] = norms[i].y;
        m_mesh.normals[i*3+2] = norms[i].z;
        m_mesh.colors[i*4+0] = cols[i].r;
        m_mesh.colors[i*4+1] = cols[i].g;
        m_mesh.colors[i*4+2] = cols[i].b;
        m_mesh.colors[i*4+3] = cols[i].a;
    }

    auto tUploadStart = std::chrono::high_resolution_clock::now();
    UploadMesh(&m_mesh, false);
    auto tUploadEnd = std::chrono::high_resolution_clock::now();

    auto tLoadStart = std::chrono::high_resolution_clock::now();
    m_model = LoadModelFromMesh(m_mesh);
    auto tLoadEnd = std::chrono::high_resolution_clock::now();

    long meshMs = std::chrono::duration_cast<std::chrono::milliseconds>(tMeshEnd - tMeshStart).count();
    long uploadMs = std::chrono::duration_cast<std::chrono::milliseconds>(tUploadEnd - tUploadStart).count();
    long loadMs = std::chrono::duration_cast<std::chrono::milliseconds>(tLoadEnd - tLoadStart).count();
    long totalMs = meshMs + uploadMs + loadMs;

    std::cout << "[BUILD PERF] Chunk (" << m_x << "," << m_z << ")" << std::endl;
    std::cout << "  CreateTerrainMesh: " << meshMs << " ms" << std::endl;
    std::cout << "  UploadMesh: " << uploadMs << " ms" << std::endl;
    std::cout << "  LoadModelFromMesh: " << loadMs << " ms" << std::endl;
    std::cout << "  BuildMesh total: " << totalMs << " ms" << std::endl;
    std::cout << "  vertices=" << vertexCount << " triangles=" << triangleCount << std::endl;
}

void Chunk::AddQuadTilted(std::vector<Vector3>& vertices, std::vector<Vector2>& texCoords,
                    std::vector<Vector3>& normals, std::vector<Color>& colors,
                    Vector3 v1, Vector3 v2, Vector3 v3, Vector3 v4, Color color, Vector3 customNormal) {
    vertices.push_back(v1); texCoords.push_back({0.0f,0.0f}); normals.push_back(customNormal); colors.push_back(color);
    vertices.push_back(v2); texCoords.push_back({1.0f,0.0f}); normals.push_back(customNormal); colors.push_back(color);
    vertices.push_back(v3); texCoords.push_back({1.0f,1.0f}); normals.push_back(customNormal); colors.push_back(color);
    vertices.push_back(v1); texCoords.push_back({0.0f,0.0f}); normals.push_back(customNormal); colors.push_back(color);
    vertices.push_back(v3); texCoords.push_back({1.0f,1.0f}); normals.push_back(customNormal); colors.push_back(color);
    vertices.push_back(v4); texCoords.push_back({0.0f,1.0f}); normals.push_back(customNormal); colors.push_back(color);
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

void Chunk::AddQuad(std::vector<Vector3>& vertices, std::vector<Vector2>& texCoords,
                    std::vector<Vector3>& normals, std::vector<Color>& colors,
                    Vector3 v1, Vector3 v2, Vector3 v3, Vector3 v4, Color color, Vector3 normal) {
    vertices.push_back(v1); texCoords.push_back({0.0f,0.0f}); normals.push_back(normal); colors.push_back(color);
    vertices.push_back(v2); texCoords.push_back({1.0f,0.0f}); normals.push_back(normal); colors.push_back(color);
    vertices.push_back(v3); texCoords.push_back({1.0f,1.0f}); normals.push_back(normal); colors.push_back(color);
    vertices.push_back(v1); texCoords.push_back({0.0f,0.0f}); normals.push_back(normal); colors.push_back(color);
    vertices.push_back(v3); texCoords.push_back({1.0f,1.0f}); normals.push_back(normal); colors.push_back(color);
    vertices.push_back(v4); texCoords.push_back({0.0f,1.0f}); normals.push_back(normal); colors.push_back(color);
}

// NOTE: Chunk generation uses worldX/worldZ + seed; identical input -> identical output across boundaries.
