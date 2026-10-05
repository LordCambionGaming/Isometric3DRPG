#pragma once
#include <raylib.h>
#include <vector>
#include "core/Character.h"
#include <algorithm>

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

    // Visual only: turns toward targetWorld (XZ) and plays the Attack clip once, then returns to Walk/Idle.
    // Returns false and does nothing if there are no valid clips or an attack is already playing.
    // Movement, pathfinding and combat rules are not touched.
    bool StartAttack(Vector3 targetWorld);
    bool IsAttacking() const { return m_animState == AnimState::Attack; }

    // Character sheet (gameplay data, no visuals). Filled once by the Character Creation flow.
    // The Player is rebuilt by Game::CreateGameSession(), so a new game never inherits an old sheet.
    void SetCharacter(const core::Character& character) { m_character = character; }
    const core::Character& GetCharacter() const { return m_character; }
    core::Character& GetCharacterMutable() { return m_character; }

    void TakeDamage(int amount) {
        m_character.vitals.currentHP =
            std::max(0, m_character.vitals.currentHP - amount);
    }

    void Heal(int amount) {
        m_character.vitals.currentHP =
            std::min(m_character.MaxHP(), m_character.vitals.currentHP + amount);
    }
    bool IsAlive() const { return m_character.vitals.currentHP > 0; }

private:
    enum class AnimState { Idle, Walk, Attack };

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

    // Clip indices inside player_animated.glb (read from the file: 0 Idle, 1 Walk, 2 Attack).
    static constexpr int kAnimIdle = 0;
    static constexpr int kAnimWalk = 1;
    static constexpr int kAnimAttack = 2;
    static constexpr int kAnimClipCount = 3;
    static constexpr float kIdleDurationSec = 2.4f;    // Idle loop length in the GLB
    static constexpr float kAttackDurationSec = 0.8f;  // Attack clip length in the GLB
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
    AnimState m_animState = AnimState::Idle;
    float m_animPhase = 0.0f;  // 0..1 inside the current clip
    int m_appliedClip = -1;    // last pose uploaded to the shared model
    int m_appliedFrame = -1;
    float m_visualYaw = 0.0f;  // radians around +Y; 0 = the model's native facing (+Z). Visual only.
    float m_attackYaw = 0.0f;

    core::Character m_character; // gameplay data, see SetCharacter()

    void UpdateVisual(float dt, const Vector3& posBefore);
    void ApplyClipPose(int clip, float phase);
    void RotateToward(float targetYaw, float maxStep);
    float ModelScale() const;
};