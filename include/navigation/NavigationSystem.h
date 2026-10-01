#pragma once

#include <vector>
#include <raylib.h>

class World;

struct TileCoord {
    int x;
    int z;

    TileCoord() : x(0), z(0) {}
    TileCoord(int _x, int _z) : x(_x), z(_z) {}

    bool operator==(const TileCoord& other) const {
        return x == other.x && z == other.z;
    }
};

class NavigationSystem {
public:
    NavigationSystem();

    void Initialize(World* world, float maxStepHeight);

    // Check if a tile is walkable
    bool IsWalkable(int tileX, int tileZ) const;

    // Check if a tile is blocked by obstacle/structure (future: trees, walls, etc.)
    bool IsTileBlocked(int tileX, int tileZ) const;

    // Check if Player can traverse from one tile to adjacent tile
    bool CanTraverse(int fromX, int fromZ, int toX, int toZ) const;

    // Find path using A* (returns empty vector if no path exists)
    std::vector<TileCoord> FindPath(int startX, int startZ, int targetX, int targetZ) const;

private:
    World* m_world;
    float m_maxStepHeight;

    // A* helpers
    struct Node {
        TileCoord coord;
        float g; // cost from start
        float h; // heuristic to target
        float f; // g + h
        TileCoord parent;
        bool hasParent;

        Node() : g(0), h(0), f(0), hasParent(false) {}
        Node(TileCoord c) : coord(c), g(0), h(0), f(0), hasParent(false) {}
    };

    float Heuristic(int x1, int z1, int x2, int z2) const;
    void GetNeighbors(const TileCoord& tile, std::vector<TileCoord>& neighbors) const;
    std::vector<TileCoord> ReconstructPath(const std::vector<Node>& allNodes, const TileCoord& start, const TileCoord& target) const;
};
