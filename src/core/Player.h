#pragma once
#include <vector>
#include <raylib.h>

// Minimal Player — stub for D&D grid movement
class Player {
public:
    Player();
    void MoveTo(int tileX, int tileZ); // world tile coordinates
    Vector3 GetWorldPosition() const;
    int GetTileX() const { return m_tileX; }
    int GetTileZ() const { return m_tileZ; }
private:
    int m_tileX, m_tileZ;
};
