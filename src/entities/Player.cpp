#include "entities/Player.h"
#include "rendering/ItemVisuals.h"
#include <raymath.h>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <core/Items.h>

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

    for (auto it = m_arrowProjectiles.begin(); it != m_arrowProjectiles.end();) {
        if (!it->Update(dt)) it = m_arrowProjectiles.erase(it);
        else ++it;
    }
}

void Player::MoveBy(Vector3 displacement, float deltaTime) {
    const Vector3 posBefore = m_position;
    m_path.clear();
    m_pathIndex = 0;
    const float distance = std::sqrt(displacement.x * displacement.x + displacement.z * displacement.z);
    if (distance > 0.0001f) {
        m_position.x += displacement.x;
        m_position.y += displacement.y;
        m_position.z += displacement.z;
        m_target = { m_position.x + displacement.x, m_position.y, m_position.z + displacement.z };
        m_moving = true;
    } else {
        m_target = m_position;
        m_moving = false;
    }
    UpdateVisual(deltaTime, posBefore);
}

void Player::SetModel(Model* model) {
    m_model = nullptr;
    m_handBoneR = -1;
    m_handBoneL = -1;
    m_chestBone = -1;
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

    // Hand bones: where the items held in the hands are attached.
    for (int i = 0; i < model->boneCount; ++i) {
        if (std::strcmp(model->bones[i].name, "Hand.R") == 0) m_handBoneR = i;
        else if (std::strcmp(model->bones[i].name, "Hand.L") == 0) m_handBoneL = i;
        else if (std::strcmp(model->bones[i].name, "Chest") == 0) m_chestBone = i;
    }
    if (m_handBoneR < 0 || m_handBoneL < 0) {
        TraceLog(LOG_WARNING, "[PLAYER MODEL] bone Hand.R/Hand.L not found (R=%d L=%d): items will not be drawn in the hands",
                 m_handBoneR, m_handBoneL);
    }

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
    m_clipOk.fill(false);
    m_animState = AnimState::Idle;
    m_animPhase = 0.0f;
    m_appliedClip = -1;
    m_appliedFrame = -1;

    if (!m_model || !anims || count < kAnimRequiredCount) {
        TraceLog(LOG_WARNING, "[PLAYER ANIM] need a model and at least %d clips (Idle, Walk, Attack), got %d: static pose", kAnimRequiredCount, count);
        return;
    }
    for (int i = 0; i < kAnimClipCount && i < count; ++i) {
        m_clipOk[static_cast<size_t>(i)] = anims[i].frameCount > 0 && IsModelAnimationValid(*m_model, anims[i]);
    }
    for (int i = 0; i < kAnimRequiredCount; ++i) {
        if (!m_clipOk[static_cast<size_t>(i)]) {
            TraceLog(LOG_WARNING, "[PLAYER ANIM] required clip %d does not match the player model: static pose", i);
            return;
        }
    }
    m_animsValid = true;
    TraceLog(LOG_INFO, "[PLAYER ANIM] %d clips in file (Idle=%d Walk=%d Attack=%d frames); Hurt %s, Attack1H %s, Attack2H %s, Bow %s",
             count, anims[kAnimIdle].frameCount, anims[kAnimWalk].frameCount, anims[kAnimAttack].frameCount,
             HasClip(kAnimHurt) ? "ok" : "missing", HasClip(kAnimAttack1H) ? "ok" : "missing",
             HasClip(kAnimAttack2H) ? "ok" : "missing", HasClip(kAnimBow) ? "ok" : "missing");
    ApplyClipPose(kAnimIdle, 0.0f);
}

float Player::ClipDuration(int clip) {
    switch (clip) {
        case kAnimIdle:     return kIdleDurationSec;
        case kAnimHurt:     return kHurtDurationSec;
        case kAnimAttack1H: return kAttack1HDurationSec;
        case kAnimAttack2H: return kAttack2HDurationSec;
        case kAnimBow:      return kBowDurationSec;
        default:            return kAttackDurationSec;
    }
}

// The grip of the equipped weapon picks the clip. A clip missing from the model falls back to the unarmed Attack.
int Player::AttackClipFor(core::WeaponGrip grip) const {
    int clip = kAnimAttack;
    switch (grip) {
        case core::WeaponGrip::OneHanded: clip = kAnimAttack1H; break;
        case core::WeaponGrip::TwoHanded: clip = kAnimAttack2H; break;
        case core::WeaponGrip::Bow:       clip = kAnimBow;      break;
        case core::WeaponGrip::Unarmed:   clip = kAnimAttack;   break;
    }
    return HasClip(clip) ? clip : kAnimAttack;
}

bool Player::StartAttack(Vector3 targetWorld) {
    if (!m_model || !m_animsValid || m_animState == AnimState::Attack || m_animState == AnimState::Hurt) return false;
    if (m_character.ActiveWeapon().grip == core::WeaponGrip::Bow &&
        m_character.QuiverArrowCount() == 0) return false;
    const float dx = targetWorld.x - m_position.x;
    const float dz = targetWorld.z - m_position.z;
    // Target on top of the player: keep the current facing.
    m_attackYaw = (dx * dx + dz * dz > 1e-6f) ? std::atan2(dx, dz) : m_visualYaw;
    m_attackTarget = targetWorld;
    m_bowReleaseFired = false;
    m_playClip = AttackClipFor(m_character.ActiveWeapon().grip);
    m_animState = AnimState::Attack;
    m_animPhase = 0.0f;
    return true;
}

void Player::PlayHurt() {
    if (!m_model || !m_animsValid || !HasClip(kAnimHurt)) return;
    m_playClip = kAnimHurt;
    m_animState = AnimState::Hurt;   // interrupts an attack in progress
    m_animPhase = 0.0f;
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
    } else if (moving && m_animState != AnimState::Hurt) {
        const float tx = m_target.x - m_position.x;
        const float tz = m_target.z - m_position.z;
        if (tx * tx + tz * tz > 1e-6f) {
            RotateToward(std::atan2(tx, tz), kWalkTurnSpeedDeg * DEG2RAD * dt);
        }
    }

    if (!m_animsValid) return; // no clips: static pose, but it still turns

    // --- Animation state machine: Idle <-> Walk; Attack and Hurt play once, then fall back to Walk/Idle
    bool bowRelease = false;
    if (m_animState == AnimState::Attack || m_animState == AnimState::Hurt) {
        const float previousPhase = m_animPhase;
        m_animPhase += dt / ClipDuration(m_playClip);
        const float releasePhase = 1.02f / kBowDurationSec;
        if (m_animState == AnimState::Attack && m_playClip == kAnimBow && !m_bowReleaseFired &&
            previousPhase < releasePhase && m_animPhase >= releasePhase) {
            bowRelease = true;
            m_bowReleaseFired = true;
        }
        if (m_animPhase >= 1.0f) {
            m_animState = moving ? AnimState::Walk : AnimState::Idle;
            m_animPhase = 0.0f;
        }
    }
    if (m_animState != AnimState::Attack && m_animState != AnimState::Hurt) {
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
        case AnimState::Attack:
        case AnimState::Hurt:   ApplyClipPose(m_playClip, m_animPhase); break;
        case AnimState::Walk:   ApplyClipPose(kAnimWalk, m_animPhase); break;
        default:                ApplyClipPose(kAnimIdle, m_animPhase); break;
    }

    if (bowRelease) {
        const float scale = ModelScale();
        const Vector3 drawPos = { m_position.x, m_position.y - m_modelBounds.min.y * scale, m_position.z };
        Matrix hand;
        if (HandToWorld(m_handBoneL, drawPos, scale, hand) && m_character.RemoveQuiverArrow()) {
            Vector3 target = m_attackTarget;
            target.y += 0.65f;
            const Vector3 handPosition = { hand.m12, hand.m13, hand.m14 };
            const Vector3 direction = Vector3Normalize(Vector3Subtract(target, handPosition));
            const Vector3 arrowCenter = Vector3Add(handPosition, Vector3Scale(direction, 0.40f));
            m_arrowProjectiles.emplace_back(arrowCenter, target);
        }
    }
}

bool Player::HandToWorld(int bone, const Vector3& drawPos, float scale, Matrix& out) const {
    if (!m_model || bone < 0 || bone >= m_model->boneCount) return false;

    // Raylib exposes animation and bind-pose transforms in MODEL space after composing parent joints.
    Transform pose;
    if (m_animsValid && m_anims && m_appliedClip >= 0 && m_appliedFrame >= 0) {
        pose = m_anims[m_appliedClip].framePoses[m_appliedFrame][bone];
    } else if (m_model->bindPose) {
        pose = m_model->bindPose[bone];
    } else {
        return false;
    }

    const Matrix bone2model = MatrixMultiply(
        MatrixMultiply(MatrixScale(pose.scale.x, pose.scale.y, pose.scale.z), QuaternionToMatrix(pose.rotation)),
        MatrixTranslate(pose.translation.x, pose.translation.y, pose.translation.z));

    // Same composition DrawModelEx uses in Render(): scale, then yaw around +Y, then translation.
    const Matrix model2world = MatrixMultiply(
        MatrixMultiply(MatrixScale(scale, scale, scale), MatrixRotate(Vector3{ 0.0f, 1.0f, 0.0f }, m_visualYaw)),
        MatrixTranslate(drawPos.x, drawPos.y, drawPos.z));
    out = MatrixMultiply(bone2model, model2world);
    return true;
}

void Player::RenderHeldItems(const Vector3& drawPos, float scale) const {
    if (!m_itemVisuals) return;

    // The bow clip of the player drives the bow's own animation (string, limbs, nocked arrow).
    const float bowPhase = (m_animState == AnimState::Attack && m_playClip == kAnimBow) ? m_animPhase : -1.0f;

    struct Slot { core::ItemId id; int bone; };
    const Slot slots[2] = { { m_character.equipment.rightHand, m_handBoneR },
                            { m_character.equipment.leftHand,  m_handBoneL } };
    bool bowDrawn = false;
    for (const Slot& s : slots) {
        if (s.id == core::kNoItem) continue;
        int bone = s.bone;
        if (ItemVisuals::UsesLeftHand(s.id)) {      // a bow is always held in the left hand
            if (bowDrawn) continue;
            bowDrawn = true;
            bone = m_handBoneL;
        }
        Matrix hand;
        if (HandToWorld(bone, drawPos, scale, hand)) {
            if (ItemVisuals::UsesLeftHand(s.id)) {
                const Matrix bowRotation = MatrixMultiply(
                    MatrixMultiply(
                        MatrixRotateX(90.0f * DEG2RAD),
                        MatrixRotateZ(90.0f * DEG2RAD)),
                    MatrixRotateY(180.0f * DEG2RAD));
                hand = MatrixMultiply(bowRotation, hand);
            }
            m_itemVisuals->DrawInHand(s.id, hand, bowPhase);
        }
    }

    Matrix quiverToWorld = MatrixIdentity();
    bool quiverVisible = false;
    if (m_character.QuiverArrowCount() > 0) {
        Matrix chest;
        if (HandToWorld(m_chestBone, drawPos, scale, chest)) {
            const Matrix quiverLocal = MatrixMultiply(
                MatrixRotateX(-90.0f * DEG2RAD),
                MatrixTranslate(-0.04f, -0.38f, -0.16f));
            quiverToWorld = MatrixMultiply(quiverLocal, chest);
            quiverVisible = true;
            m_itemVisuals->DrawInHand(core::items::Quiver, quiverToWorld);
        }
    }

    const float pickupStart = 0.20f;
    const float nockTime = 0.65f;
    const float bowTime = bowPhase * kBowDurationSec;
    if (quiverVisible && bowTime >= pickupStart && bowTime < nockTime) {
        Matrix hand;
        if (HandToWorld(m_handBoneR, drawPos, scale, hand)) {
            Vector3 target = m_attackTarget;
            target.y += 0.65f;
            const Vector3 handPosition = { hand.m12, hand.m13, hand.m14 };
            const Vector3 aimDirection = Vector3Normalize(Vector3Subtract(target, handPosition));
            const Vector3 quiverBottom = Vector3Transform(Vector3{ 0.0f, 0.0f, 0.0f }, quiverToWorld);
            const Vector3 quiverAxis = Vector3Normalize(Vector3Subtract(
                Vector3Transform(Vector3{ 0.0f, 0.0f, 1.0f }, quiverToWorld), quiverBottom));
            const Vector3 pickupPosition = Vector3Transform(Vector3{ 0.0f, 0.0f, 0.735f }, quiverToWorld);
            const Vector3 arrowInHand = Vector3Add(handPosition, Vector3Scale(aimDirection, 0.40f));
            const float amount = std::clamp((bowTime - pickupStart) / (nockTime - pickupStart), 0.0f, 1.0f);
            const float eased = amount * amount * (3.0f - 2.0f * amount);
            const Vector3 arrowCenter = Vector3Lerp(pickupPosition, arrowInHand, eased);
            const Vector3 arrowDirection = Vector3Normalize(Vector3Lerp(quiverAxis, aimDirection, eased));
            const Quaternion rotation = QuaternionFromVector3ToVector3(Vector3{ 0.0f, 0.0f, 1.0f }, arrowDirection);
            const Matrix arrowToWorld = MatrixMultiply(
                QuaternionToMatrix(rotation),
                MatrixTranslate(arrowCenter.x, arrowCenter.y, arrowCenter.z));
            m_itemVisuals->DrawInHand(core::items::Arrow, arrowToWorld);
        }
    }

    for (const ArrowProjectile& arrow : m_arrowProjectiles) arrow.Render(m_itemVisuals);
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
        RenderHeldItems(drawPos, scale);
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