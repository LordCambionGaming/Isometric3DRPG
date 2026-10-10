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

Chunk::Chunk(int x, int z, int size, float tileSize)
    : m_x(x), m_z(z), m_size(size), m_tileSize(tileSize)
 , m_generated(false), m_hasMesh(false)
{
    m_tiles.resize(size * size);
}
Chunk::~Chunk() { Unload(); }
void Chunk::Generate(const TerrainGenerator& terrainGenerator, World* world) {
    if (m_generated) return;
    GenerateTerrain(terrainGenerator, world);
    GenerateColors();
    m_generated = true;
}
void Chunk::BuildMesh() {
    if (!m_generated || m_hasMesh) return;
    CreateTerrainMesh();
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
            if (tile.hasWater) continue;
            int hh = (int)tile.height; if (hh < 1) hh = 1;
            float y = hh * s + 0.02f;
            float minX = x * s; float maxX = (x + 1) * s;
            float minZ = z * s; float maxZ = (z + 1) * s;
            m_gridLines.push_back({minX, y, minZ}); m_gridLines.push_back({maxX, y, minZ});
            m_gridLines.push_back({maxX, y, minZ}); m_gridLines.push_back({maxX, y, maxZ});
            m_gridLines.push_back({maxX, y, maxZ}); m_gridLines.push_back({minX, y, maxZ});
            m_gridLines.push_back({minX, y, maxZ}); m_gridLines.push_back({minX, y, minZ});
        }
    }
}
void Chunk::Render() {
    if (!m_hasMesh) return;
    DrawModel(m_model, GetWorldPosition(), 1.0f, WHITE);
    RenderGrid();
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
    if (m_hasMesh) { UnloadModel(m_model); m_hasMesh = false; }
    m_gridLines.clear(); m_gridLines.shrink_to_fit(); m_generated = false;
}
Vector3 Chunk::GetWorldPosition() const { return { m_x * m_size * m_tileSize, 0.0f, m_z * m_size * m_tileSize }; }
float Chunk::GetHeightAt(float localX, float localZ) const {
    if (!m_generated) return 0.0f;
    int tileX = (int)(localX / m_tileSize); int tileZ = (int)(localZ / m_tileSize);
    if (tileX < 0 || tileX >= m_size || tileZ < 0 || tileZ >= m_size) return 0.0f;
    return GetTile(tileX, tileZ).height;
}
void Chunk::GenerateTerrain(const TerrainGenerator& terrainGenerator, World* world) {
    const int chunkOffsetX = m_x * m_size;
    const int chunkOffsetZ = m_z * m_size;

    WorldFeatureGenerator featureGen;
    featureGen.Initialize(terrainGenerator.GetSeed());

    for (int z = 0; z < m_size; z++) {
        for (int x = 0; x < m_size; x++) {
            int index = z * m_size + x;
            TerrainTile& tile = m_tiles[index];
            int worldX = chunkOffsetX + x;
            int worldZ = chunkOffsetZ + z;

            // 1. genera normale
            int level = terrainGenerator.GenerateHeightLevel(worldX, worldZ);
            WaterFeature wf = featureGen.GetFeatureAt(worldX, worldZ, terrainGenerator);
            bool isWater = (wf==WaterFeature::Sea)||(wf==WaterFeature::Lake)||(wf==WaterFeature::River);

            if (isWater) {
                tile.height = 1.0f;
                tile.hasWater = true;
                tile.waterHeight = 2.0f;
                tile.waterFeature = (int)wf;
            } else {
                if(level < 2) level = 2;
                tile.height = (float)level;
                tile.hasWater = false;
                tile.waterHeight = 0.0f;
                tile.waterFeature = 0;
            }

            // 2. OVERRIDE: se siamo dentro un villaggio, appiattisci a 2.0
            if (world) {
                float flatH = 0.0f;
                // worldX + 0.5 per centro tile
                if (world->GetFlattenedHeight(worldX + 0.5f, worldZ + 0.5f, flatH)) {
                    tile.height = flatH; // sempre 2.0
                    tile.hasWater = false;
                    tile.waterHeight = 0.0f;
                    tile.waterFeature = 0;
                }
            }

            tile.temperature = terrainGenerator.GenerateTemperature(worldX, worldZ);
            tile.humidity = terrainGenerator.GenerateHumidity(worldX, worldZ);
            tile.biomeType = 0;
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
    verts.reserve(m_size*m_size*24); texs.reserve(m_size*m_size*24); norms.reserve(m_size*m_size*24); cols.reserve(m_size*m_size*24);
    float s = m_tileSize;
for (int z = 0; z < m_size; ++z) {
 for (int x = 0; x < m_size; ++x) {
  int level = (int)GetTile(x,z).height; if(level<1) level=1;
  float minX=x*s, maxX=(x+1)*s, minZ=z*s, maxZ=(z+1)*s;
  int nl=(x>0)?(int)GetTile(x-1,z).height:level;
  int nr=(x<m_size-1)?(int)GetTile(x+1,z).height:level;
  int nb=(z>0)?(int)GetTile(x,z-1).height:level;
  int nf=(z<m_size-1)?(int)GetTile(x,z+1).height:level;
  Color topColor={34,139,34,255}; Color bodyColor={160,120,60,255};
  bool hasWater=GetTile(x,z).hasWater;
  
  if (hasWater) {
    float yWater=2.0f*s;
    AddQuadTilted(verts,texs,norms,cols,{minX,yWater,minZ},{minX,yWater,maxZ},{maxX,yWater,maxZ},{maxX,yWater,minZ},Color{30,144,255,255},{0,1,0});
  } else {
    // SOLO se non è acqua disegna prato verde
    float yTop = level*s;
    AddQuadTilted(verts,texs,norms,cols,{minX,yTop,minZ},{minX,yTop,maxZ},{maxX,yTop,maxZ},{maxX,yTop,minZ},topColor,{0,1,0});
  }
  // lati sempre marroni
  for(int y=0;y<level;++y){
    float yBase=y*s, yTop=yBase+s;
    if (nl!=level||y==0||y==level-1) AddQuad(verts,texs,norms,cols,{minX,yBase,minZ},{minX,yBase,maxZ},{minX,yTop,maxZ},{minX,yTop,minZ},bodyColor,{-1,0,0});
    if (nr!=level||y==0||y==level-1) AddQuad(verts,texs,norms,cols,{maxX,yBase,minZ},{maxX,yTop,minZ},{maxX,yTop,maxZ},{maxX,yBase,maxZ},bodyColor,{1,0,0});
    if (nb!=level||y==0||y==level-1) AddQuad(verts,texs,norms,cols,{minX,yBase,maxZ},{maxX,yBase,maxZ},{maxX,yTop,maxZ},{minX,yTop,maxZ},bodyColor,{0,0,1});
    if (nf!=level||y==0||y==level-1) AddQuad(verts,texs,norms,cols,{minX,yBase,minZ},{minX,yTop,minZ},{maxX,yTop,minZ},{maxX,yBase,minZ},bodyColor,{0,0,-1});
  }
 }
}
    m_mesh={0}; int vc=(int)verts.size(); m_mesh.vertexCount=vc; m_mesh.triangleCount=vc/3;
    m_mesh.vertices=(float*)MemAlloc(vc*3*sizeof(float)); m_mesh.texcoords=(float*)MemAlloc(vc*2*sizeof(float));
    m_mesh.normals=(float*)MemAlloc(vc*3*sizeof(float)); m_mesh.colors=(unsigned char*)MemAlloc(vc*4*sizeof(unsigned char));
    for(int i=0;i<vc;i++){ m_mesh.vertices[i*3+0]=verts[i].x; m_mesh.vertices[i*3+1]=verts[i].y; m_mesh.vertices[i*3+2]=verts[i].z; m_mesh.texcoords[i*2+0]=texs[i].x; m_mesh.texcoords[i*2+1]=texs[i].y; m_mesh.normals[i*3+0]=norms[i].x; m_mesh.normals[i*3+1]=norms[i].y; m_mesh.normals[i*3+2]=norms[i].z; m_mesh.colors[i*4+0]=cols[i].r; m_mesh.colors[i*4+1]=cols[i].g; m_mesh.colors[i*4+2]=cols[i].b; m_mesh.colors[i*4+3]=cols[i].a; }
    UploadMesh(&m_mesh,false); m_model=LoadModelFromMesh(m_mesh); if(m_hasShader) m_model.materials[0].shader=m_shader;
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