#include "world/Chunk.h"
#include "world/TerrainGenerator.h"
#include <raylib.h>
#include <raymath.h>
#include <vector>
#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

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

void Chunk::Generate(const TerrainGenerator& terrainGenerator) {
    if (m_generated) return;
    GenerateTerrain(terrainGenerator);
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

void Chunk::GenerateTerrain(const TerrainGenerator& terrainGenerator) {
    for (int z = 0; z < m_size; z++) {
        for (int x = 0; x < m_size; x++) {
            int worldX = m_x * m_size + x;
            int worldZ = m_z * m_size + z;
            TerrainTile& tile = GetTile(x, z);
            int level = terrainGenerator.GenerateHeightLevel(worldX, worldZ);
            tile.height = static_cast<float>(level);
            // Climate from generator (for future biome linkage)
            tile.temperature = terrainGenerator.GenerateTemperature(worldX, worldZ);
            tile.humidity = terrainGenerator.GenerateHumidity(worldX, worldZ);
            tile.biomeType = 0;
            tile.hasVegetation = false;
        }
    }

    // ----- DEBUG: print height maps and distributions -----
    // Collect height stats for this chunk
    int minHeight = 999, maxHeight = -1;
    int hist[6] = {0,0,0,0,0,0}; // livelli 1-5

    for (int z = 0; z < m_size; z++) {
        for (int x = 0; x < m_size; x++) {
            int h = static_cast<int>(GetTile(x, z).height);
            if (h < 1) h = 1;
            if (h < minHeight) minHeight = h;
            if (h > maxHeight) maxHeight = h;
            if (h >= 1 && h <= 6) hist[h-1]++;
        }
    }

    // Only print for a few representative chunks (-1,0), (0,0), (1,0), (0,-1), (0,1)
    // We'll detect these by checking if any of these coordinates match
    bool printDebug = false;
    if (m_x == 0 && m_z == 0) {
        // Check if we have already printed for (0,0) or need to print again for other relevant chunks
        // We'll print for all 5 chunks, but mark which ones we print for clarity
        if (m_x == 0 && m_z == 0) printDebug = true;
    }
    // Simplified: Print for chunks where (m_x == -1 || m_x == 0 || m_x == 1) && (m_z == -1 || m_z == 0 || m_z == 1)
    // but avoid duplicate excessive output, so we'll print only once for each of the 5 positions
    static bool printedMinusOneZero = false;
    static bool printedZeroZero = false;
    static bool printedPlusOneZero = false;
    static bool printedZeroMinusOne = false;
    static bool printedZeroPlusOne = false;

    if (m_x == -1 && m_z == 0 && !printedMinusOneZero) {
        printDebug = true;
        printedMinusOneZero = true;
    } else if (m_x == 0 && m_z == 0 && !printedZeroZero) {
        printDebug = true;
        printedZeroZero = true;
    } else if (m_x == 1 && m_z == 0 && !printedPlusOneZero) {
        printDebug = true;
        printedPlusOneZero = true;
    } else if (m_x == 0 && m_z == -1 && !printedZeroMinusOne) {
        printDebug = true;
        printedZeroMinusOne = true;
    } else if (m_x == 0 && m_z == 1 && !printedZeroPlusOne) {
        printDebug = true;
        printedZeroPlusOne = true;
    }

    if (printDebug) {
        std::ofstream logFile("debug/terrain_generation.log", std::ios::out | std::ios::app);
        if (logFile.is_open()) {
            logFile << "=== CHUNK " << m_x << "," << m_z << " === seed=" << terrainGenerator.GetSeed() << "\n";
            logFile << "minHeight=" << minHeight << " maxHeight=" << maxHeight << "\n";
            for (int i = 0; i < 5; i++) logFile << "level" << (i+1) << "=" << hist[i] << " ";
            logFile << "\n";
            if (m_x == 0 && m_z == 0) {
                logFile << "HEIGHT MAP (0,0) 16x16:\n";
                for (int z = 0; z < m_size; z++) {
                    for (int x = 0; x < m_size; x++) {
                        int h = static_cast<int>(GetTile(x, z).height);
                        if (h < 1) h = 1;
                        logFile << h << " ";
                    }
                    logFile << "\n";
                }
                logFile << "Intermediate samples (worldX=8+8, 32+8, 8+32):\n";
                int samples[3][2] = {{8,8}, {32,8}, {8,32}};
                for (int s = 0; s < 3; s++) {
                    int wx = m_x*m_size + samples[s][0];
                    int wz = m_z*m_size + samples[s][1];
                    logFile << "(" << wx << "," << wz << ") reg=" << terrainGenerator.GetRegion(wx,wz) << "\n";
                }
            }
            logFile << "\n";
        }
        // Print 16x16 height map for (0,0) only (skip for other chunks to save output)
        if (m_x == 0 && m_z == 0) {
            std::cout << "\n=== HEIGHT MAP CHUNK 0,0 (16x16) ===\n";
            for (int z = 0; z < m_size; z++) {
                for (int x = 0; x < m_size; x++) {
                    int h = static_cast<int>(GetTile(x, z).height);
                    if (h < 1) h = 1;
                    if (h < 10) std::cout << " " << h;
                    else std::cout << " " << h;
                    std::cout << " ";
                }
                std::cout << "\n";
            }
        }

        std::cout << "[Chunk " << m_x << "," << m_z << "] minHeight = " << minHeight
                  << " maxHeight = " << maxHeight;
        for (int i = 0; i < 5; i++) {
            std::cout << " level " << i+1 << " = " << hist[i];
        }
        std::cout << "\n";
    }
    // ----- END DEBUG -----
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
    std::vector<Vector3> verts;
    std::vector<Vector2> texs;
    std::vector<Vector3> norms;
    std::vector<Color> cols;
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
            Color topColor = { 34, 139, 34, 255 };    // verde erba
            Color bodyColor = { 160, 120, 60, 255 }; // marrone strato
            Color faceColor = bodyColor;
            for (int y = 0; y < level; ++y) {
                float yBase = baseY + y * s;
                float yTop  = yBase + s;
                // Side face color: if this layer is the top of this column, use green
                Color sideColor = (y == level - 1) ? topColor : faceColor;
                // Top face — FIX winding (v2/v4 scambiati), normale +Y
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
    UploadMesh(&m_mesh, false);
    m_model = LoadModelFromMesh(m_mesh);
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
