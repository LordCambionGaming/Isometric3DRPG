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
    float s = m_tileSize;

    // 1) Blocks (solid cubes at tile positions)
    for (int z = 0; z < m_size; z++) {
        for (int x = 0; x < m_size; x++) {
            int h = static_cast<int>(GetTile(x, z).height);
            if (h < 1) h = 1;
            Color c = (h == 1) ? GREEN : (h == 2) ? BROWN : (h == 3) ? GRAY : LIGHTGRAY;
            DrawCube({(float)x * s + s*0.5f, (float)h * s * 0.5f, (float)z * s + s*0.5f},
                     s, (float)h * s, s, c);
        }
    }

    // 2) D&D tile grid: 17 vertical + 17 horizontal lines at each tile's top surface
    for (int i = 0; i <= m_size; i++) {
        float xPos = i * s;
        // Vertical lines: from z=0 to z=m_size*s, y at tile top
        for (int z = 0; z <= m_size; z++) {
            float zPos = z * s;
            // Find nearest tile to get height for this grid segment
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
        float zPos = j * s;
        // Horizontal lines: from x=0 to x=m_size*s, y at tile top
        for (int x = 0; x <= m_size; x++) {
            float xPos = x * s;
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

void Chunk::GenerateTerrain(unsigned int seed) {
    int map[8][8] = {
        {1,1,1,1,1,1,1,1},
        {1,1,2,2,2,2,1,1},
        {1,1,2,3,3,2,1,1},
        {1,1,2,3,3,2,1,1},
        {1,1,2,3,3,2,1,1},
        {1,1,2,2,2,2,1,1},
        {1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1}
    };
    for (int z = 0; z < m_size && z < 8; z++) {
        for (int x = 0; x < m_size && x < 8; x++) {
            TerrainTile& tile = GetTile(x, z);
            int level = (x < 8 && z < 8) ? map[z][x] : 1;
            tile.height = static_cast<float>(level);
            tile.biomeType = 0;
            tile.hasVegetation = false;
        }
    }
    for (int z = 0; z < m_size; z++) {
        for (int x = 0; x < m_size; x++) {
            if (x < 8 && z < 8) continue;
            GetTile(x, z).height = 1.0f;
            GetTile(x, z).biomeType = 0;
            GetTile(x, z).hasVegetation = false;
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
            if (level <= 0) level = 1;
            float baseY = 0.0f;
            float minX = x * s; float maxX = (x + 1) * s;
            float minZ = z * s; float maxZ = (z + 1) * s;
            int neighborLeft = (x > 0) ? static_cast<int>(GetTile(x-1, z).height) : level;
            int neighborRight = (x < m_size - 1) ? static_cast<int>(GetTile(x+1, z).height) : level;
            int neighborBack = (z > 0) ? static_cast<int>(GetTile(x, z-1).height) : level;
            int neighborFront = (z < m_size - 1) ? static_cast<int>(GetTile(x, z+1).height) : level;
            Color blockColor;
            if (level == 1) blockColor = { 34, 139, 34, 255 };
            else if (level == 2) blockColor = { 160, 120, 60, 255 };
            else if (level == 3) blockColor = { 120, 120, 120, 255 };
            else blockColor = { 200, 200, 230, 255 };
            for (int y = 0; y < level; ++y) {
                float yBase = baseY + y * s;
                float yTop  = yBase + s;
                AddQuad(verts, texs, norms, cols,
                    {minX, yTop, minZ}, {maxX, yTop, minZ}, {maxX, yTop, maxZ}, {minX, yTop, maxZ},
                    blockColor);
                int drawLeft = (neighborLeft < level) ? 1 : (y == 0 ? 1 : 0);
                if (drawLeft) {
                    AddQuad(verts, texs, norms, cols,
                        {minX, yBase, minZ}, {minX, yBase, maxZ}, {minX, yTop, maxZ}, {minX, yTop, minZ},
                        blockColor);
                }
                int drawRight = (neighborRight < level) ? 1 : (y == 0 ? 1 : 0);
                if (drawRight) {
                    AddQuad(verts, texs, norms, cols,
                        {maxX, yBase, minZ}, {maxX, yTop, minZ}, {maxX, yTop, maxZ}, {maxX, yBase, maxZ},
                        blockColor);
                }
                int drawBack = (neighborBack < level) ? 1 : (y == 0 ? 1 : 0);
                if (drawBack) {
                    AddQuad(verts, texs, norms, cols,
                        {minX, yBase, maxZ}, {maxX, yBase, maxZ}, {maxX, yTop, maxZ}, {minX, yTop, maxZ},
                        blockColor);
                }
                int drawFront = (neighborFront < level) ? 1 : (y == 0 ? 1 : 0);
                if (drawFront) {
                    AddQuad(verts, texs, norms, cols,
                        {minX, yBase, minZ}, {minX, yTop, minZ}, {maxX, yTop, minZ}, {maxX, yBase, minZ},
                        blockColor);
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
                    Vector3 v1, Vector3 v2, Vector3 v3, Vector3 v4, Color color) {
    Vector3 normal = { 0.0f, 1.0f, 0.0f };
    vertices.push_back(v1); texCoords.push_back({0.0f,0.0f}); normals.push_back(normal); colors.push_back(color);
    vertices.push_back(v2); texCoords.push_back({1.0f,0.0f}); normals.push_back(normal); colors.push_back(color);
    vertices.push_back(v3); texCoords.push_back({1.0f,1.0f}); normals.push_back(normal); colors.push_back(color);
    vertices.push_back(v1); texCoords.push_back({0.0f,0.0f}); normals.push_back(normal); colors.push_back(color);
    vertices.push_back(v3); texCoords.push_back({1.0f,1.0f}); normals.push_back(normal); colors.push_back(color);
    vertices.push_back(v4); texCoords.push_back({0.0f,1.0f}); normals.push_back(normal); colors.push_back(color);
}
