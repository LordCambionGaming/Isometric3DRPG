#include "core/Player.h"

Player::Player() : m_tileX(0), m_tileZ(0) {}

void Player::MoveTo(int tileX, int tileZ) {
    m_tileX = tileX;
    m_tileZ = tileZ;
}

Vector3 Player::GetWorldPosition() const {
    return {
        (float)(m_tileX * 1.0f),
        1.0f,
        (float)(m_tileZ * 1.0f)
    };
}
