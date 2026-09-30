#pragma once
#include <raylib.h>

class Player {
public:
    Player();
    void Update(float deltaTime);
    void Render() const;
    Vector3 GetPosition() const { return m_position; }
    int GetChunkX() const;
    int GetChunkZ() const;
    void SetPosition(Vector3 pos) { m_position = pos; }
    void MoveToTile(int tileX, int tileZ);
private:
    Vector3 m_position;
    Vector3 m_target;
    bool m_moving;
};
