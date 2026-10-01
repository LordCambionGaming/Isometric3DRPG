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
        const PathNode& node = m_path[m_pathIndex];
        float targetX = node.x * m_tileSize + m_tileSize * 0.5f;
        float targetZ = node.z * m_tileSize + m_tileSize * 0.5f;
        float targetY = node.y;

        float dx = targetX - m_position.x;
        float dz = targetZ - m_position.z;
        float dy = targetY - m_position.y;
        float distXZ = std::sqrt(dx * dx + dz * dz);

        float step = m_speed * dt;
        // If very close to target in XZ and Y is near target, snap to exact waypoint
        bool nearX = std::abs(dx) < 0.05f;
        bool nearZ = std::abs(dz) < 0.05f;
        bool nearY = std::abs(dy) < 0.05f;
        if ((nearX && nearZ && nearY) || (distXZ <= step && std::abs(dy) <= step)) {
            m_position.x = targetX;
            m_position.z = targetZ;
            m_position.y = targetY; // snap exactly to surface
            m_pathIndex++;

            if (m_pathIndex < m_path.size()) {
                const PathNode& next = m_path[m_pathIndex];
                m_target.x = next.x * m_tileSize + m_tileSize * 0.5f;
                m_target.y = next.y;
                m_target.z = next.z * m_tileSize + m_tileSize * 0.5f;
            } else {
                m_path.clear();
                m_pathIndex = 0;
                m_moving = false;
            }
        } else {
            // Interpolate XZ along plane direction
            float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
            float moveStep = std::min(step, dist);
            float ratio = (dist > 0.001f) ? moveStep / dist : 0.0f;
            m_position.x += dx * ratio;
            m_position.y += dy * ratio;
            m_position.z += dz * ratio;
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
            float ratio = step / dist;
            m_position.x += dx * ratio;
            m_position.y += dy * ratio;
            m_position.z += dz * ratio;
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

void Player::SetPath(const std::vector<PathNode>& path) {
    m_path = path;
    m_pathIndex = 0;
    if (!m_path.empty()) {
        // Skip start tile if it matches current position (avoid 0-distance move)
        int curTileX = GetTileX();
        int curTileZ = GetTileZ();
        if (m_path[0].x == curTileX && m_path[0].z == curTileZ) {
            m_pathIndex = 1; // jump to first real waypoint
        }
        if (m_pathIndex < m_path.size()) {
            const PathNode& node = m_path[m_pathIndex];
            m_target.x = node.x * m_tileSize + m_tileSize * 0.5f;
            m_target.y = node.y;
            m_target.z = node.z * m_tileSize + m_tileSize * 0.5f;
            m_moving = true;
        } else {
            m_path.clear();
            m_pathIndex = 0;
            m_moving = false;
        }
    }
}
