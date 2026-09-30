#pragma once

#include <raylib.h>
#include <vector>
#include "world/WorldFeatureGenerator.h"
#include <memory>

class TerrainGenerator;

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
    float waterHeight = 2.0f; // WATER_LEVEL = 2
    int waterFeature = 0; // 0=Dry, 1=Lake, 2=River
    int tileWaterCount = 0;
};

class Chunk {
public:
    Chunk(int x, int z, int size, float tileSize);
    ~Chunk();

    void Generate(const TerrainGenerator& terrainGenerator);
    void BuildMesh();
    void Render();
    void Unload();

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
    void GenerateTerrain(const TerrainGenerator& terrainGenerator);
    void GenerateColors();
    void CreateTerrainMesh();
    void AddQuad(std::vector<Vector3>& vertices, std::vector<Vector2>& texCoords,
                 std::vector<Vector3>& normals, std::vector<Color>& colors,
                 Vector3 v1, Vector3 v2, Vector3 v3, Vector3 v4, Color color, Vector3 normal);
    void AddQuadTilted(std::vector<Vector3>& vertices, std::vector<Vector2>& texCoords,
                 std::vector<Vector3>& normals, std::vector<Color>& colors,
                 Vector3 v1, Vector3 v2, Vector3 v3, Vector3 v4, Color color, Vector3 customNormal);

    bool ShouldHaveVegetation(int biomeType, float randomValue) const;
    void GenerateTileColors(TerrainTile& tile) const;

    // Enhanced mesh generation
    Color BlendColors(Color color1, Color color2, float factor) const;
    void AddEnhancedQuad(std::vector<Vector3>& vertices, std::vector<Vector2>& texCoords,
                        std::vector<Vector3>& normals, std::vector<Color>& colors,
                        Vector3 v1, Vector3 v2, Vector3 v3, Vector3 v4, Color color, const TerrainTile& tile);

    int m_x, m_z;           // Chunk coordinates
    int m_size;             // Size in tiles (e.g., 16x16)
    float m_tileSize;       // Size of each tile in world units

    std::vector<TerrainTile> m_tiles;

    // Rendering
    Mesh m_mesh;
    Model m_model;
    bool m_generated;
    bool m_hasMesh;

    // Terrain generation parameters
    static constexpr float NOISE_SCALE = 0.05f;
    static constexpr float HEIGHT_MULTIPLIER = 5.0f;
};