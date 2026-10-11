#include "world/Chunk.h"
#include "world/TerrainGenerator.h"
#include "world/World.h"
#include "world/WorldFeatureGenerator.h"
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <vector>
#include <cmath>
#include <algorithm>

static Color LerpCol(Color a, Color b, float t) {
    t = Clamp(t, 0.0f, 1.0f);
    return { (unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
             (unsigned char)(a.b + (b.b - a.b) * t), (unsigned char)(a.a + (b.a - a.a) * t) };
}

Chunk::Chunk(int x, int z, int size, float tileSize)
    : m_x(x), m_z(z), m_size(size), m_tileSize(tileSize)
 , m_generated(false), m_hasMesh(false)
{
    m_tiles.resize(size * size);
}
Chunk::~Chunk() { Unload(); }
void Chunk::Generate(const TerrainGenerator& terrainGenerator, World* world) {
    m_world = world;
    if (m_generated) return;
    GenerateTerrain(terrainGenerator, world);
    GenerateColors();
    m_generated = true;
}
void Chunk::BuildMesh() {
    if (!m_generated) return;
    if (m_hasMesh)      { UnloadModel(m_model);      m_hasMesh = false; }
    if (m_hasWaterMesh) { UnloadModel(m_waterModel); m_hasWaterMesh = false; }
    CreateTerrainMesh();   // opachi: terra + fondale sabbia + lati
    CreateWaterMesh();     // trasparenti: solo top acqua
    BuildGridLines();
    m_hasMesh = true;
}

void Chunk::BuildGridLines() {
    m_gridLines.clear();
    m_gridLines.reserve(m_size * m_size * 8);
    float s = m_tileSize;
    for (int z = 0; z < m_size; z++) {
        for (int x = 0; x < m_size; x++) {
            const TerrainTile& tile = GetTile(x, z);
            if (tile.hasWater) continue;  // niente griglia in acqua, basta
            int hh = (int)tile.height; if (hh < 1) hh = 1;
            float y = hh * s + 0.02f;
            float minX = x * s, maxX = (x + 1) * s;
            float minZ = z * s, maxZ = (z + 1) * s;
            m_gridLines.push_back({minX, y, minZ}); m_gridLines.push_back({maxX, y, minZ});
            m_gridLines.push_back({maxX, y, minZ}); m_gridLines.push_back({maxX, y, maxZ});
            m_gridLines.push_back({maxX, y, maxZ}); m_gridLines.push_back({minX, y, maxZ});
            m_gridLines.push_back({minX, y, maxZ}); m_gridLines.push_back({minX, y, minZ});
        }
    }
}

void Chunk::Render() {
    if (m_hasMesh) DrawModel(m_model, GetWorldPosition(), 1.0f, WHITE);
    RenderGrid();
}

void Chunk::RenderWater() {
    if (m_hasWaterMesh) DrawModel(m_waterModel, GetWorldPosition(), 1.0f, WHITE);
}

void Chunk::RenderGrid() const {
    if (m_gridLines.empty()) return;
    Vector3 wp = GetWorldPosition();
    rlPushMatrix(); rlTranslatef(wp.x, wp.y, wp.z);
    rlBegin(RL_LINES); rlColor4ub(80, 80, 80, 90);
    for (auto& v : m_gridLines) rlVertex3f(v.x, v.y, v.z);
    rlEnd(); rlPopMatrix();
}
void Chunk::Unload() {
    if (m_hasMesh)      { UnloadModel(m_model);      m_hasMesh = false; }
    if (m_hasWaterMesh) { UnloadModel(m_waterModel); m_hasWaterMesh = false; }
    m_gridLines.clear(); m_gridLines.shrink_to_fit();
    m_generated = false;
}
Vector3 Chunk::GetWorldPosition() const { return { m_x * m_size * m_tileSize, 0.0f, m_z * m_size * m_tileSize }; }

float Chunk::GetHeightAt(float localX, float localZ) const {
    if (!m_generated) return 0.0f;
    int tileX = (int)(localX / m_tileSize);
    int tileZ = (int)(localZ / m_tileSize);
    if (tileX < 0 || tileX >= m_size || tileZ < 0 || tileZ >= m_size) return 0.0f;
    const TerrainTile& t = GetTile(tileX, tileZ);
    return t.hasWater ? t.waterHeight : t.height;
}

void Chunk::GenerateTerrain(const TerrainGenerator& terrainGenerator, World* world) {
    const int chunkOffsetX = m_x * m_size;
    const int chunkOffsetZ = m_z * m_size;

    for (int z = 0; z < m_size; z++) {
        for (int x = 0; x < m_size; x++) {
            int index = z * m_size + x;
            TerrainTile& tile = m_tiles[index];
            int worldX = chunkOffsetX + x;
            int worldZ = chunkOffsetZ + z;

            int level = terrainGenerator.GenerateHeightLevel(worldX, worldZ);

            // Acqua = terreno sotto il livello del mare
            tile.height = (float)level;
            if (level < TerrainGenerator::SEA_LEVEL) {
                tile.hasWater    = true;
                tile.waterHeight = (float)TerrainGenerator::SEA_LEVEL;
                tile.waterFeature = 1;
            } else {
                tile.hasWater    = false;
                tile.waterHeight = 0.0f;
                tile.waterFeature = 0;
            }

            // Override: dentro un villaggio, appiattisci e asciuga
            if (world) {
                float flatH = 0.0f;
                if (world->GetFlattenedHeight(worldX + 0.5f, worldZ + 0.5f, flatH)) {
                    tile.height      = flatH;
                    tile.hasWater    = false;
                    tile.waterHeight = 0.0f;
                    tile.waterFeature = 0;
                }
            }

            tile.temperature = terrainGenerator.GenerateTemperature(worldX, worldZ);
            tile.humidity    = terrainGenerator.GenerateHumidity(worldX, worldZ);
            tile.biomeType   = 0;
            tile.hasVegetation = false;
        }
    }
}
void Chunk::GenerateColors() { for (int z=0;z<m_size;z++) for(int x=0;x<m_size;x++) GenerateTileColors(GetTile(x,z)); }
bool Chunk::ShouldHaveVegetation(int biomeType, float randomValue) const {
    switch (biomeType) { case 0: return false; case 1: return randomValue<0.1f; case 2: return randomValue<0.6f; case 3: return randomValue<0.9f; case 4: return randomValue<0.4f; case 5: return randomValue<0.2f; case 6: return randomValue<0.05f; case 7: return randomValue<0.8f; default: return randomValue<0.3f; } }
void Chunk::GenerateTileColors(TerrainTile& tile) const {
    switch (tile.biomeType) { case 0: tile.baseColor=DARKBLUE; tile.detailColor=BLUE; break; case 1: tile.baseColor=BEIGE; tile.detailColor=BROWN; break; case 2: tile.baseColor=GREEN; tile.detailColor=LIME; break; case 3: tile.baseColor=DARKGREEN; tile.detailColor=GREEN; break; case 4: tile.baseColor=BROWN; tile.detailColor=DARKBROWN; break; case 5: tile.baseColor=GRAY; tile.detailColor=LIGHTGRAY; break; case 6: tile.baseColor=GOLD; tile.detailColor=YELLOW; break; case 7: tile.baseColor=DARKGREEN; tile.detailColor=BROWN; break; default: tile.baseColor=GREEN; tile.detailColor=GREEN; break; } }
void Chunk::CreateTerrainMesh() {
    std::vector<Vector3> verts; std::vector<Vector2> texs; std::vector<Vector3> norms; std::vector<Color> cols;
    verts.reserve(m_size*m_size*32); texs.reserve(m_size*m_size*32); norms.reserve(m_size*m_size*32); cols.reserve(m_size*m_size*32);

    const float s = m_tileSize;
    const Color grassColor = { 34, 139,  34, 255};
    const Color earthColor = {160, 120,  60, 255};
    const Color sandColor  = {210, 180, 120, 255};
    const Color waterSide  = { 30, 100, 170, 255}; // blu opaco, per lati terra sotto il pelo
    const int maxDepth = TerrainGenerator::SEA_LEVEL - 1;

    for (int z = 0; z < m_size; ++z) {
        for (int x = 0; x < m_size; ++x) {
            const TerrainTile& t = GetTile(x, z);
            int level = (int)t.height; if (level < 1) level = 1;
            const bool hasWater = t.hasWater;
            const int waterH = hasWater ? (int)t.waterHeight : 0;

            const float minX = x * s, maxX = (x + 1) * s;
            const float minZ = z * s, maxZ = (z + 1) * s;

            if (hasWater) {
    const Color floorDeep = { 35, 70, 115, 255 };
    auto fc = [&](int vx, int vz) {
        float d = Clamp(VertexWaterDepth(vx, vz) / (float)maxDepth, 0.0f, 1.0f);
        return LerpCol(sandColor, floorDeep, d);
    };
    float yFloor = (float)(waterH - maxDepth) * s;
    AddQuadColors(verts, texs, norms, cols,
        {minX, yFloor, minZ}, {minX, yFloor, maxZ}, {maxX, yFloor, maxZ}, {maxX, yFloor, minZ},
        fc(x, z), fc(x, z + 1), fc(x + 1, z + 1), fc(x + 1, z), {0, 1, 0});
} else {
                // Top prato
                float yT = (float)level * s;
                AddQuadTilted(verts, texs, norms, cols,
                    {minX, yT, minZ}, {minX, yT, maxZ}, {maxX, yT, maxZ}, {maxX, yT, minZ},
                    grassColor, {0, 1, 0});

                // Base del lato: se vicino è acqua -> waterH (così il pelo copre);
                // se vicino è terra -> altezza vicino; se fuori chunk -> level (nessun lato).
                auto baseFor = [&](int xx, int zz) -> int {
    if (xx < 0 || xx >= m_size || zz < 0 || zz >= m_size) {
        if (!m_world) return level;
        int nh; bool nw;
        m_world->GetTileSurface(m_x * m_size + xx, m_z * m_size + zz, nh, nw);
        if (nw) return 0;
        nh = std::max(nh, 1);
        return (nh > level) ? level : nh;
    }
    const TerrainTile& n = GetTile(xx, zz);
    if (n.hasWater) return 0;
    return ((int)n.height > level) ? level : (int)n.height;
};
                int bXm = baseFor(x-1, z);
                int bXp = baseFor(x+1, z);
                int bZm = baseFor(x, z-1);
                int bZp = baseFor(x, z+1);

                for (int y = 0; y < level; ++y) {
                    float yB = y * s, yT2 = yB + s;
                    // Sotto il pelo dell'acqua -> blu (sommerso). Sopra -> terra marrone.
                    Color sideColor = (y >= TerrainGenerator::SEA_LEVEL) ? earthColor : waterSide;
                    if (bXm < y + 1) AddQuad(verts, texs, norms, cols,
                        {minX, yB, minZ}, {minX, yB, maxZ}, {minX, yT2, maxZ}, {minX, yT2, minZ},
                        sideColor, {-1, 0, 0});
                    if (bXp < y + 1) AddQuad(verts, texs, norms, cols,
                        {maxX, yB, minZ}, {maxX, yT2, minZ}, {maxX, yT2, maxZ}, {maxX, yB, maxZ},
                        sideColor, { 1, 0, 0});
                   if (bZm < y + 1) AddQuad(verts, texs, norms, cols,
    {minX, yB, minZ}, {minX, yT2, minZ}, {maxX, yT2, minZ}, {maxX, yB, minZ},
    sideColor, {0, 0, -1});
if (bZp < y + 1) AddQuad(verts, texs, norms, cols,
    {minX, yB, maxZ}, {maxX, yB, maxZ}, {maxX, yT2, maxZ}, {minX, yT2, maxZ},
    sideColor, {0, 0, 1});
                }
            }
        }
    }

    m_mesh = {0};
    int vc = (int)verts.size();
    m_mesh.vertexCount   = vc;
    m_mesh.triangleCount = vc / 3;
    m_mesh.vertices  = (float*)MemAlloc(vc * 3 * sizeof(float));
    m_mesh.texcoords = (float*)MemAlloc(vc * 2 * sizeof(float));
    m_mesh.normals   = (float*)MemAlloc(vc * 3 * sizeof(float));
    m_mesh.colors    = (unsigned char*)MemAlloc(vc * 4 * sizeof(unsigned char));
    for (int i = 0; i < vc; i++) {
        m_mesh.vertices[i*3+0]  = verts[i].x;  m_mesh.vertices[i*3+1]  = verts[i].y;  m_mesh.vertices[i*3+2]  = verts[i].z;
        m_mesh.texcoords[i*2+0] = texs[i].x;   m_mesh.texcoords[i*2+1] = texs[i].y;
        m_mesh.normals[i*3+0]   = norms[i].x;  m_mesh.normals[i*3+1]   = norms[i].y;  m_mesh.normals[i*3+2]   = norms[i].z;
        m_mesh.colors[i*4+0]    = cols[i].r;   m_mesh.colors[i*4+1]    = cols[i].g;
        m_mesh.colors[i*4+2]    = cols[i].b;   m_mesh.colors[i*4+3]    = cols[i].a;
    }
    UploadMesh(&m_mesh, false);
    m_model = LoadModelFromMesh(m_mesh);
    if (m_hasShader) m_model.materials[0].shader = m_shader;
}

void Chunk::CreateWaterMesh() {
    std::vector<Vector3> verts; std::vector<Vector2> texs; std::vector<Vector3> norms; std::vector<Color> cols;
    verts.reserve(m_size*m_size*6); texs.reserve(m_size*m_size*6); norms.reserve(m_size*m_size*6); cols.reserve(m_size*m_size*6);

    const float s = m_tileSize;
    const int maxDepth = TerrainGenerator::SEA_LEVEL - 1;

    const Color shallow = {  40, 190, 215, 110 };   // turchese, abbastanza coprente da non far virare la sabbia al grigio
    const Color deep    = {  20,  60, 130, 235 };

    // Colore dell'acqua in un vertice (vx,vz) della griglia del chunk, in base alla profondità media attorno
    auto wc = [&](int vx, int vz) {
        float d = Clamp(VertexWaterDepth(vx, vz) / (float)maxDepth, 0.0f, 1.0f);
        return LerpCol(shallow, deep, d);
    };

    for (int z = 0; z < m_size; ++z) {
        for (int x = 0; x < m_size; ++x) {
            const TerrainTile& t = GetTile(x, z);
            if (!t.hasWater) continue;
            int waterH = (int)t.waterHeight;

            const float minX = x * s, maxX = (x + 1) * s;
            const float minZ = z * s, maxZ = (z + 1) * s;
            float yW = (float)waterH * s;

            AddQuadColors(verts, texs, norms, cols,
                {minX, yW, minZ}, {minX, yW, maxZ}, {maxX, yW, maxZ}, {maxX, yW, minZ},
                wc(x, z), wc(x, z + 1), wc(x + 1, z + 1), wc(x + 1, z), {0, 1, 0});
        }
    }

    m_waterMesh = {0};
    int vc = (int)verts.size();
    m_waterMesh.vertexCount   = vc;
    m_waterMesh.triangleCount = vc / 3;
    if (vc > 0) {
        m_waterMesh.vertices  = (float*)MemAlloc(vc * 3 * sizeof(float));
        m_waterMesh.texcoords = (float*)MemAlloc(vc * 2 * sizeof(float));
        m_waterMesh.normals   = (float*)MemAlloc(vc * 3 * sizeof(float));
        m_waterMesh.colors    = (unsigned char*)MemAlloc(vc * 4 * sizeof(unsigned char));
        for (int i = 0; i < vc; i++) {
            m_waterMesh.vertices[i*3+0]  = verts[i].x;  m_waterMesh.vertices[i*3+1]  = verts[i].y;  m_waterMesh.vertices[i*3+2]  = verts[i].z;
            m_waterMesh.texcoords[i*2+0] = texs[i].x;   m_waterMesh.texcoords[i*2+1] = texs[i].y;
            m_waterMesh.normals[i*3+0]   = norms[i].x;  m_waterMesh.normals[i*3+1]   = norms[i].y;  m_waterMesh.normals[i*3+2]   = norms[i].z;
            m_waterMesh.colors[i*4+0]    = cols[i].r;   m_waterMesh.colors[i*4+1]    = cols[i].g;
            m_waterMesh.colors[i*4+2]    = cols[i].b;   m_waterMesh.colors[i*4+3]    = cols[i].a;
        }
        UploadMesh(&m_waterMesh, false);
        m_waterModel = LoadModelFromMesh(m_waterMesh);
        if (m_hasShader) m_waterModel.materials[0].shader = m_shader;
        m_hasWaterMesh = true;
    }
}


// Profondità media nel vertice (vx,vz) di 0..m_size: i tile di terra contano 0, quindi alla riva sfuma.
float Chunk::VertexWaterDepth(int vx, int vz) const {
    float sum = 0.0f;
    for (int dz = -1; dz <= 0; ++dz)
        for (int dx = -1; dx <= 0; ++dx) {
            int tx = std::clamp(vx + dx, 0, m_size - 1);
            int tz = std::clamp(vz + dz, 0, m_size - 1);
            const TerrainTile& t = GetTile(tx, tz);
            if (!t.hasWater) continue;
            int level = std::max((int)t.height, 1);
            sum += (float)((int)t.waterHeight - level);
        }
    return sum * 0.25f;
}

void Chunk::AddQuadColors(std::vector<Vector3>& v, std::vector<Vector2>& t, std::vector<Vector3>& n,
                          std::vector<Color>& c, Vector3 v1, Vector3 v2, Vector3 v3, Vector3 v4,
                          Color c1, Color c2, Color c3, Color c4, Vector3 nr) {
    v.push_back(v1); t.push_back({0,0}); n.push_back(nr); c.push_back(c1);
    v.push_back(v2); t.push_back({1,0}); n.push_back(nr); c.push_back(c2);
    v.push_back(v3); t.push_back({1,1}); n.push_back(nr); c.push_back(c3);
    v.push_back(v1); t.push_back({0,0}); n.push_back(nr); c.push_back(c1);
    v.push_back(v3); t.push_back({1,1}); n.push_back(nr); c.push_back(c3);
    v.push_back(v4); t.push_back({0,1}); n.push_back(nr); c.push_back(c4);
}

void Chunk::AddQuadTilted(std::vector<Vector3>& vertices, std::vector<Vector2>& texCoords, std::vector<Vector3>& normals, std::vector<Color>& colors, Vector3 v1, Vector3 v2, Vector3 v3, Vector3 v4, Color color, Vector3 customNormal) {
    vertices.push_back(v1); texCoords.push_back({0,0}); normals.push_back(customNormal); colors.push_back(color);
    vertices.push_back(v2); texCoords.push_back({1,0}); normals.push_back(customNormal); colors.push_back(color);
    vertices.push_back(v3); texCoords.push_back({1,1}); normals.push_back(customNormal); colors.push_back(color);
    vertices.push_back(v1); texCoords.push_back({0,0}); normals.push_back(customNormal); colors.push_back(color);
    vertices.push_back(v3); texCoords.push_back({1,1}); normals.push_back(customNormal); colors.push_back(color);
    vertices.push_back(v4); texCoords.push_back({0,1}); normals.push_back(customNormal); colors.push_back(color);
}
Color Chunk::BlendColors(Color c1, Color c2, float f) const { f=Clamp(f,0.0f,1.0f); return {(unsigned char)(c1.r+(c2.r-c1.r)*f),(unsigned char)(c1.g+(c2.g-c1.g)*f),(unsigned char)(c1.b+(c2.b-c1.b)*f),255}; }
void Chunk::AddQuad(std::vector<Vector3>& vertices, std::vector<Vector2>& texCoords, std::vector<Vector3>& normals, std::vector<Color>& colors, Vector3 v1, Vector3 v2, Vector3 v3, Vector3 v4, Color color, Vector3 normal) { AddQuadTilted(vertices, texCoords, normals, colors, v1, v2, v3, v4, color, normal); }