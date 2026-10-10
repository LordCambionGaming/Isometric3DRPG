#pragma once

#include <raylib.h>
#include <vector>
#include "world/WorldFeatureGenerator.h"

class TerrainGenerator;
class World;

struct TerrainTile {
    float height = 0.0f;
    float temperature = 0.5f;
    float humidity = 0.5f;
    int biomeType = 0;
    Color baseColor = GREEN;
    Color detailColor = GREEN;
    float roughness = 0.5f;
    bool hasVegetation = false;
    bool hasWater = false;
    float waterHeight = 2.0f;
    int waterFeature = 0;
    int tileWaterCount = 0;
};

class Chunk {
public:
    Chunk(int x, int z, int size, float tileSize);
    ~Chunk();

    void Generate(const TerrainGenerator& terrainGenerator, World* world = nullptr);
    void BuildMesh();
    void Render();
    void Unload();

    void SetShader(Shader shader) { m_shader = shader; m_hasShader = true; }

    bool IsGenerated() const { return m_generated; }
    bool HasMesh() const { return m_hasMesh; }

    int GetX() const { return m_x; }
    int GetZ() const { return m_z; }
    int GetSize() const { return m_size; }

    Vector3 GetWorldPosition() const;
    TerrainTile& GetTile(int x, int z) { return m_tiles[z * m_size + x]; }
    const TerrainTile& GetTile(int x, int z) const { return m_tiles[z * m_size + x]; }

    float GetHeightAt(float localX, float localZ) const;

private:
    void GenerateTerrain(const TerrainGenerator& terrainGenerator, World* world = nullptr);
    void GenerateColors();
    void CreateTerrainMesh();
    void BuildGridLines();
    void RenderGrid() const;

    void AddQuad(std::vector<Vector3>& vertices, std::vector<Vector2>& texCoords,
                 std::vector<Vector3>& normals, std::vector<Color>& colors,
                 Vector3 v1, Vector3 v2, Vector3 v3, Vector3 v4, Color color, Vector3 normal);
    void AddQuadTilted(std::vector<Vector3>& vertices, std::vector<Vector2>& texCoords,
                 std::vector<Vector3>& normals, std::vector<Color>& colors,
                 Vector3 v1, Vector3 v2, Vector3 v3, Vector3 v4, Color color, Vector3 customNormal);

    bool ShouldHaveVegetation(int biomeType, float randomValue) const;
    void GenerateTileColors(TerrainTile& tile) const;
    Color BlendColors(Color color1, Color color2, float factor) const;

    int m_x, m_z;
    int m_size;
    float m_tileSize;

    std::vector<TerrainTile> m_tiles;
    std::vector<Vector3> m_gridLines;

    Mesh m_mesh{};
    Model m_model{};
    bool m_generated;
    bool m_hasMesh;
    Shader m_shader{};
    bool m_hasShader = false;

    static constexpr float NOISE_SCALE = 0.05f;
    static constexpr float HEIGHT_MULTIPLIER = 5.0f;
};