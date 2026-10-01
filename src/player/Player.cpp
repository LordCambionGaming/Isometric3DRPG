#include "player/Player.h"
#include <raymath.h>
#include <cmath>

Player::Player() {
    m_position.x = 0; m_position.y = 0; m_position.z = 0;
    m_target.x = 0; m_target.y = 0; m_target.z = 0;
    m_moving = false;
    m_pathIndex = 0;
}

void Player::Update(float dt) {
    if (!m_path.empty() && m_pathIndex < m_path.size()) {
        int tx = m_path[m_pathIndex].x;
        int tz = m_path[m_pathIndex].z;
        float targetX = tx * m_tileSize + m_tileSize * 0.5f;
        float targetZ = tz * m_tileSize + m_tileSize * 0.5f;

        float dx = targetX - m_position.x;
        float dz = targetZ - m_position.z;
        float dist = std::sqrt(dx * dx + dz * dz);

        float step = m_speed * dt;
        if (dist <= step || dist < 0.01f) {
            // Reached this tile: snap to center, advance index, set next target
            m_position.x = targetX;
            m_position.z = targetZ;
            m_pathIndex++;

            if (m_pathIndex < m_path.size()) {
                int nextTx = m_path[m_pathIndex].x;
                int nextTz = m_path[m_pathIndex].z;
                m_target.x = nextTx * m_tileSize + m_tileSize * 0.5f;
                m_target.z = nextTz * m_tileSize + m_tileSize * 0.5f;
            } else {
                // Path finished
                m_path.clear();
                m_pathIndex = 0;
                m_moving = false;
            }
        } else {
            // Move toward tile center
            m_position.x += (dx / dist) * step;
            m_position.z += (dz / dist) * step;
        }
    } else if (m_moving) {
        // Fallback direct movement (MoveToTile)
        float dx = m_target.x - m_position.x;
        float dy = m_target.y - m_position.y;
        float dz = m_target.z - m_position.z;
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        float step = m_speed * dt;
        if (dist <= step || dist < 0.01f) {
            m_position = m_target;
            m_moving = false;
        } else {
            m_position.x += (dx / dist) * step;
            m_position.y += (dy / dist) * step;
            m_position.z += (dz / dist) * step;
        }
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

int Player::GetTileX() const {
    return (int)std::floor(m_position.x / m_tileSize);
}

int Player::GetTileZ() const {
    return (int)std::floor(m_position.z / m_tileSize);
}

void Player::MoveToTile(int tx, int tz, float surfaceY) {
    // Tile center: tx*tileSize + tileSize/2
    m_target.x = tx * m_tileSize + m_tileSize * 0.5f;
    m_target.z = tz * m_tileSize + m_tileSize * 0.5f;
    m_target.y = surfaceY;
    m_moving = true;
}

void Player::SetPath(const std::vector<TileCoord>& path) {
    m_path = path;
    m_pathIndex = 0;
    if (!m_path.empty()) {
        int tx = m_path[0].x;
        int tz = m_path[0].z;
        // Set first target; surface height will be updated in Update when needed
        m_target.x = tx * m_tileSize + m_tileSize * 0.5f;
        m_target.z = tz * m_tileSize + m_tileSize * 0.5f;
        m_target.y = m_position.y; // keep current height initially; overridden when reaching tile
        m_moving = true;
    }
}
