#pragma once
#include <raylib.h>
#include <array>
#include <vector>
#include "core/Character.h"
#include "entities/ArrowProjectile.h"
#include <algorithm>

class ItemVisuals;   // rendering/ItemVisuals.h

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
    // Rendering only: world point just above the top of the head (used by the HUD indicator).
    Vector3 GetIndicatorAnchor() const;
    void MoveToTile(int tileX, int tileZ, float surfaceY);
    void SetPath(const std::vector<PathNode>& path);

    // Visual only. Non-owning: the Model is owned/cached by AssetManager and shared.
    // Passing nullptr (or an invalid model) keeps the debug-cube fallback.
    // Non-const because skeletal animation rewrites the shared model's skinned vertex buffers.
    void SetModel(Model* model);

    // Visual only. Non-owning: the clip array is owned/cached by AssetManager. Call AFTER SetModel().
    // Expects the player_animated.glb clip order (kAnimIdle/kAnimWalk/kAnimAttack below). If the clips do
    // not match the model the player keeps rendering the static pose (it still turns to face its direction).
    void SetAnimations(ModelAnimation* anims, int count);

    // Visual only. Non-owning: models of the items held in the hands (equipment.rightHand / leftHand).
    // nullptr = nothing is drawn in the hands. Call AFTER SetModel(), it can be set at any time.
    void SetItemVisuals(ItemVisuals* visuals) { m_itemVisuals = visuals; }

    // Visual only: turns toward targetWorld (XZ) and plays the attack clip of the EQUIPPED weapon once
    // (no weapon: Attack, one hand: Attack1H, two hands: Attack2H, bow: Bow), then returns to Walk/Idle.
    // Returns false and does nothing if there are no valid clips, or an attack / hurt clip is already playing.
    // Movement, pathfinding and combat rules are not touched.
    bool StartAttack(Vector3 targetWorld);
    bool IsAttacking() const { return m_animState == AnimState::Attack; }

    // Visual only: plays the Hurt (damage received) clip once. It interrupts an attack in progress.
    // TakeDamage() calls it; it does nothing if the loaded model has no Hurt clip.
    void PlayHurt();
    bool IsHurt() const { return m_animState == AnimState::Hurt; }

    // Character sheet (gameplay data, no visuals). Filled once by the Character Creation flow.
    // The Player is rebuilt by Game::CreateGameSession(), so a new game never inherits an old sheet.
    void SetCharacter(const core::Character& character) { m_character = character; }
    const core::Character& GetCharacter() const { return m_character; }
    core::Character& GetCharacterMutable() { return m_character; }

    void TakeDamage(int amount) {
        m_character.vitals.currentHP =
            std::max(0, m_character.vitals.currentHP - amount);
        if (amount > 0) PlayHurt();   // visual flinch only
    }

    void Heal(int amount) {
        m_character.vitals.currentHP =
            std::min(m_character.MaxHP(), m_character.vitals.currentHP + amount);
    }
    bool IsAlive() const { return m_character.vitals.currentHP > 0; }

private:
    enum class AnimState { Idle, Walk, Attack, Hurt };

    std::vector<PathNode> m_path;
    size_t m_pathIndex;
    Vector3 m_position;
    Vector3 m_target;
    bool m_moving;
    float m_tileSize = 1.0f;
    float m_speed = 5.0f;

    // Visual representation (no gameplay state)
    static constexpr float kVisualHeightMeters = 1.8f;    // world height at tileSize == 1.0
    static constexpr float kVisualFootprintMeters = 0.6f; // expected max X/Z extent
    static constexpr float kIndicatorGapMeters = 0.15f;   // clearance between head top and indicator tip
    Model* m_model = nullptr;
    BoundingBox m_modelBounds = { { 0, 0, 0 }, { 0, 0, 0 } }; // local-space, measured once in SetModel

    // Clip indices inside player_animated.glb (read from the file's animation list).
    static constexpr int kAnimIdle = 0;
    static constexpr int kAnimWalk = 1;
    static constexpr int kAnimAttack = 2;     // unarmed
    static constexpr int kAnimHurt = 3;       // damage received
    static constexpr int kAnimAttack1H = 4;   // one-handed weapon (impact at 0.30 s)
    static constexpr int kAnimAttack2H = 5;   // two-handed weapon (impact at 0.50 s)
    static constexpr int kAnimBow = 6;        // bow (release at 1.02 s)
    static constexpr int kAnimClipCount = 7;      // clips in the file
    static constexpr int kAnimRequiredCount = 3;  // Idle, Walk, Attack must exist and match the model; the others are optional
    // Clip lengths in seconds, as written in the GLB. The one-shot clips are played by time.
    static constexpr float kIdleDurationSec = 2.4f;
    static constexpr float kAttackDurationSec = 0.8f;
    static constexpr float kHurtDurationSec = 0.7f;
    static constexpr float kAttack1HDurationSec = 0.75f;
    static constexpr float kAttack2HDurationSec = 1.1f;
    static constexpr float kBowDurationSec = 1.7f;
    // Metres the body travels during one full Walk cycle (two steps) at model scale 1.0, measured from the
    // GLB (stance foot travels 0.699 m per half cycle). The Walk phase advances with the distance really
    // covered each frame, so foot speed follows ground speed whatever m_speed or the slope is.
    static constexpr float kWalkCycleMeters = 1.398f;
    static constexpr float kWalkTurnSpeedDeg = 540.0f;   // yaw speed while walking
    static constexpr float kAttackTurnSpeedDeg = 900.0f; // yaw speed while turning to the attack target

    // Visual animation state (no gameplay state)
    ModelAnimation* m_anims = nullptr; // non-owning (AssetManager)
    int m_animCount = 0;
    bool m_animsValid = false;
    std::array<bool, kAnimClipCount> m_clipOk{};  // clip exists and matches the model
    AnimState m_animState = AnimState::Idle;
    int m_playClip = kAnimAttack;  // clip of the current one-shot (Attack or Hurt)
    float m_animPhase = 0.0f;  // 0..1 inside the current clip
    Vector3 m_attackTarget{};
    bool m_bowReleaseFired = false;
    std::vector<ArrowProjectile> m_arrowProjectiles;
    int m_appliedClip = -1;    // last pose uploaded to the shared model
    int m_appliedFrame = -1;
    float m_visualYaw = 0.0f;  // radians around +Y; 0 = the model's native facing (+Z). Visual only.
    float m_attackYaw = 0.0f;

    // Items held in the hands (visual only)
    ItemVisuals* m_itemVisuals = nullptr; // non-owning (Game)
    int m_handBoneR = -1;                 // indices of "Hand.R" / "Hand.L" in the model's skeleton
    int m_handBoneL = -1;
    int m_chestBone = -1;

    core::Character m_character; // gameplay data, see SetCharacter()

    void UpdateVisual(float dt, const Vector3& posBefore);
    void ApplyClipPose(int clip, float phase);
    bool HasClip(int clip) const { return clip >= 0 && clip < kAnimClipCount && m_clipOk[static_cast<size_t>(clip)]; }
    int AttackClipFor(core::WeaponGrip grip) const;   // clip for the weapon grip, falling back to the unarmed Attack
    static float ClipDuration(int clip);
    void RotateToward(float targetYaw, float maxStep);
    float ModelScale() const;

    // Draws equipment.rightHand / leftHand on the hand bones of the pose currently on the model.
    void RenderHeldItems(const Vector3& drawPos, float scale) const;
    // Hand space -> world matrix of a hand bone (current pose + the same transform DrawModelEx applies).
    bool HandToWorld(int bone, const Vector3& drawPos, float scale, Matrix& out) const;
};