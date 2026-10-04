#include "player/Player.h"
#include <raymath.h>
#include <cmath>
#include <algorithm>

Player::Player() {
    m_position.x = 0; m_position.y = 0; m_position.z = 0;
    m_target.x = 0; m_target.y = 0; m_target.z = 0;
    m_moving = false;
    m_pathIndex = 0;
}

void Player::Update(float dt) {
    const Vector3 posBefore = m_position; // visual animation only: measures the distance really covered
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

    // Visual only: reads the final position, never writes gameplay state.
    UpdateVisual(dt, posBefore);
}

void Player::SetModel(Model* model) {
    m_model = nullptr;
    // Animations must be (re)attached after the model.
    m_animsValid = false;
    m_animState = AnimState::Idle;
    m_animPhase = 0.0f;
    m_appliedClip = -1;
    m_appliedFrame = -1;
    if (!model || model->meshCount == 0) return;

    // Measure once; Render() only does a multiply per frame.
    const BoundingBox bb = GetModelBoundingBox(*model);
    const float h = bb.max.y - bb.min.y;
    if (h <= 0.0001f) {
        TraceLog(LOG_WARNING, "[PLAYER MODEL] degenerate bounding box, keeping fallback visual");
        return;
    }
    m_model = model;
    m_modelBounds = bb;

    const float scale = (kVisualHeightMeters * m_tileSize) / h;
    const float w = (bb.max.x - bb.min.x) * scale;
    const float d = (bb.max.z - bb.min.z) * scale;
    TraceLog(LOG_INFO, "[PLAYER MODEL] bbox size=(%.3f, %.3f, %.3f) min.y=%.3f -> scale=%.4f, world size=(%.3f x %.3f x %.3f)",
             bb.max.x - bb.min.x, h, bb.max.z - bb.min.z, bb.min.y, scale, w, h * scale, d);
    if (w > kVisualFootprintMeters * m_tileSize + 0.01f || d > kVisualFootprintMeters * m_tileSize + 0.01f) {
        TraceLog(LOG_WARNING, "[PLAYER MODEL] footprint %.3f x %.3f exceeds %.2f m", w, d, kVisualFootprintMeters * m_tileSize);
    }
    if (std::fabs(bb.min.y) > 0.01f) {
        TraceLog(LOG_WARNING, "[PLAYER MODEL] model origin is %.3f above/below the feet; compensated using the bounding box", bb.min.y);
    }
}

float Player::ModelScale() const {
    const float h = m_modelBounds.max.y - m_modelBounds.min.y;
    return (m_model && h > 0.0001f) ? (kVisualHeightMeters * m_tileSize) / h : 1.0f;
}

void Player::SetAnimations(ModelAnimation* anims, int count) {
    m_anims = anims;
    m_animCount = count;
    m_animsValid = false;
    m_animState = AnimState::Idle;
    m_animPhase = 0.0f;
    m_appliedClip = -1;
    m_appliedFrame = -1;

    if (!m_model || !anims || count < kAnimClipCount) {
        TraceLog(LOG_WARNING, "[PLAYER ANIM] need a model and %d clips (Idle, Walk, Attack), got %d: static pose", kAnimClipCount, count);
        return;
    }
    for (int i = 0; i < kAnimClipCount; ++i) {
        if (anims[i].frameCount <= 0 || !IsModelAnimationValid(*m_model, anims[i])) {
            TraceLog(LOG_WARNING, "[PLAYER ANIM] clip %d does not match the player model: static pose", i);
            return;
        }
    }
    m_animsValid = true;
    TraceLog(LOG_INFO, "[PLAYER ANIM] clips ok: Idle=%d, Walk=%d, Attack=%d frames (%d clips in file)",
             anims[kAnimIdle].frameCount, anims[kAnimWalk].frameCount, anims[kAnimAttack].frameCount, count);
    ApplyClipPose(kAnimIdle, 0.0f);
}

bool Player::StartAttack(Vector3 targetWorld) {
    if (!m_model || !m_animsValid || m_animState == AnimState::Attack) return false;
    const float dx = targetWorld.x - m_position.x;
    const float dz = targetWorld.z - m_position.z;
    // Target on top of the player: keep the current facing.
    m_attackYaw = (dx * dx + dz * dz > 1e-6f) ? std::atan2(dx, dz) : m_visualYaw;
    m_animState = AnimState::Attack;
    m_animPhase = 0.0f;
    return true;
}

void Player::ApplyClipPose(int clip, float phase) {
    if (!m_animsValid || !m_model) return;
    const ModelAnimation& anim = m_anims[clip];
    int frame = static_cast<int>(phase * static_cast<float>(anim.frameCount));
    frame = std::max(0, std::min(frame, anim.frameCount - 1));
    if (clip == m_appliedClip && frame == m_appliedFrame) return; // this pose is already on the model
    UpdateModelAnimation(*m_model, anim, frame);
    m_appliedClip = clip;
    m_appliedFrame = frame;
}

void Player::RotateToward(float targetYaw, float maxStep) {
    float diff = std::fmod(targetYaw - m_visualYaw, 2.0f * PI);
    if (diff > PI) diff -= 2.0f * PI;
    else if (diff < -PI) diff += 2.0f * PI;

    if (std::fabs(diff) <= maxStep) m_visualYaw = targetYaw;
    else m_visualYaw += (diff > 0.0f) ? maxStep : -maxStep;

    m_visualYaw = std::fmod(m_visualYaw, 2.0f * PI);
    if (m_visualYaw > PI) m_visualYaw -= 2.0f * PI;
    else if (m_visualYaw < -PI) m_visualYaw += 2.0f * PI;
}

void Player::UpdateVisual(float dt, const Vector3& posBefore) {
    if (!m_model) return;

    const float dx = m_position.x - posBefore.x;
    const float dz = m_position.z - posBefore.z;
    const float movedXZ = std::sqrt(dx * dx + dz * dz);
    const bool moving = m_moving || !m_path.empty(); // read-only view of the existing movement state

    // --- Facing (visual yaw only; m_position / m_target are never written here)
    if (m_animState == AnimState::Attack) {
        RotateToward(m_attackYaw, kAttackTurnSpeedDeg * DEG2RAD * dt);
    } else if (moving) {
        const float tx = m_target.x - m_position.x;
        const float tz = m_target.z - m_position.z;
        if (tx * tx + tz * tz > 1e-6f) {
            RotateToward(std::atan2(tx, tz), kWalkTurnSpeedDeg * DEG2RAD * dt);
        }
    }

    if (!m_animsValid) return; // no clips: static pose, but it still turns

    // --- Animation state machine: Idle <-> Walk, Attack plays once then falls back to Walk/Idle
    if (m_animState == AnimState::Attack) {
        m_animPhase += dt / kAttackDurationSec;
        if (m_animPhase >= 1.0f) {
            m_animState = moving ? AnimState::Walk : AnimState::Idle;
            m_animPhase = 0.0f;
        }
    }
    if (m_animState != AnimState::Attack) {
        const AnimState wanted = moving ? AnimState::Walk : AnimState::Idle;
        if (wanted != m_animState) {
            m_animState = wanted;
            m_animPhase = 0.0f;
        }
        if (m_animState == AnimState::Walk) {
            m_animPhase += movedXZ / (kWalkCycleMeters * ModelScale()); // distance-driven: no foot sliding
        } else {
            m_animPhase += dt / kIdleDurationSec;
        }
        m_animPhase -= std::floor(m_animPhase); // loop
    }

    switch (m_animState) {
        case AnimState::Attack: ApplyClipPose(kAnimAttack, m_animPhase); break;
        case AnimState::Walk:   ApplyClipPose(kAnimWalk, m_animPhase); break;
        default:                ApplyClipPose(kAnimIdle, m_animPhase); break;
    }
}

void Player::Render() const {
    Vector3 p = m_position; // UNICA fonte autorevole

    if (m_model) {
        // Uniform scale so the model's real bounding-box height equals 1.8 m (x tile size).
        const float scale = (kVisualHeightMeters * m_tileSize) / (m_modelBounds.max.y - m_modelBounds.min.y);
        // XZ: model pivot (feet centre) sits on the player position.
        // Y : lowest vertex of the model sits exactly on the player's surface height.
        const Vector3 drawPos = { p.x, p.y - m_modelBounds.min.y * scale, p.z };
        // Visual yaw only (smoothed in UpdateVisual); the gameplay position is untouched.
        DrawModelEx(*m_model, drawPos, Vector3{ 0.0f, 1.0f, 0.0f }, m_visualYaw * RAD2DEG,
                    Vector3{ scale, scale, scale }, WHITE);
        return;
    }

    // Debug: solo il cubo RED e il cubo BLUE (hover), nessun marker giallo sopra il player
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

Vector3 Player::GetIndicatorAnchor() const {
    // Same height SetModel() scales the model to, so the anchor follows the real head top.
    return { m_position.x,
             m_position.y + (kVisualHeightMeters + kIndicatorGapMeters) * m_tileSize,
             m_position.z };
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