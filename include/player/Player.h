#pragma once
#include <raylib.h>
#include <vector>

struct PathNode {
    int x;
    int z;
    float y; // surface height

    PathNode() : x(0), z(0), y(0.0f) {}
    PathNode(int _x, int _z, float _y) : x(_x), z(_z), y(_y) {}
};

class Player {
public:
    Player();
    void Update(float deltaTime);
    void Render() const;
    Vector3 GetPosition() const { return m_position; }
    int GetChunkX() const;
    int GetChunkZ() const;
    void SetPosition(Vector3 pos) { m_position = pos; }
    void SetTileSize(float size) { m_tileSize = size; }
    float GetTileSize() const { return m_tileSize; }
    int GetTileX() const;
    int GetTileZ() const;
    void MoveToTile(int tileX, int tileZ, float surfaceY);
    void SetPath(const std::vector<PathNode>& path);
private:
    std::vector<PathNode> m_path;
    size_t m_pathIndex;
    Vector3 m_position;
    Vector3 m_target;
    bool m_moving;
    float m_tileSize = 1.0f;
    float m_speed = 5.0f;
};
