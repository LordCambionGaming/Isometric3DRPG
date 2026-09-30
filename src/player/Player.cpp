#include "player/Player.h"
#include <raymath.h>
Player::Player() {
    m_position.x = 0; m_position.y = 0; m_position.z = 0;
    m_target.x = 0; m_target.y = 0; m_target.z = 0;
    m_moving = false;
}
void Player::Update(float dt) {
    if (m_moving) {
        m_position = m_target;
        m_moving = false;
    }
}
void Player::Render() const {
    Vector3 p = m_position;
    DrawCube(p, 0.5f, 1.0f, 0.5f, RED);
    Vector3 h;
    h.x = p.x; h.y = p.y + 1.0f; h.z = p.z;
    DrawCube(h, 0.3f, 0.4f, 0.3f, BLUE);
}
int Player::GetChunkX() const { return (int)(m_position.x / 16); }
int Player::GetChunkZ() const { return (int)(m_position.z / 16); }
void Player::MoveToTile(int tx, int tz) {
    m_target.x = tx * 1.0f; m_target.y = 0.0f; m_target.z = tz * 1.0f;
    m_moving = true;
}
