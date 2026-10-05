#pragma once
#include <functional>
#include <vector>
#include <raylib.h>
#include <raymath.h>
#include "core/CombatSystem.h"

namespace ui {

class DiceRoller {
public:
    enum class State { Idle, Dragging, Flying, Settled };
    using RollProvider = std::function<core::CombatRoll()>;

    DiceRoller();
    ~DiceRoller();
    DiceRoller(const DiceRoller&) = delete;
    DiceRoller& operator=(const DiceRoller&) = delete;

    void SetSides(int sides);
    void SetRollProvider(RollProvider provider) { m_provider = std::move(provider); }
    void SetBounds(Rectangle bounds) { m_bounds = bounds; }
    void Reset();

    void HandleInput();
    void Update(float dt);
    void Render();

    State GetState() const { return m_state; }
    bool IsBusy() const { return m_state == State::Dragging || m_state == State::Flying; }
    bool ConsumeResult(core::CombatRoll& out);

private:
    void BuildGeometry();
    void BuildAtlas();
    void EnsureResources();
    void DrawDie3D();
    void Integrate(float dt);
    Quaternion TargetFor(int face) const;
    Vector2 RestPos() const;
    Vector2 FaceUV(int face, int k) const;

    State m_state = State::Idle;
    Rectangle m_bounds{ 0, 0, 600, 400 };
    Vector2 m_pos{ 0, 0 };
    Vector2 m_vel{ 0, 0 };
    Vector2 m_grab{ 0, 0 };

    Quaternion m_orient{ 0, 0, 0, 1 };
    Quaternion m_target{ 0, 0, 0, 1 };
    Vector3 m_angVel{ 0, 0, 0 };
    float m_settleTime = 0.0f;

    core::CombatRoll m_pending{};
    bool m_hasResult = false;
    bool m_aligned = false;
    RollProvider m_provider;

    // --- Geometria Dinamica ---
    struct Polygon {
        std::vector<int> v;
        Vector3 n;
        int number;
    };
    int m_sides = 20;
    std::vector<Vector3> m_v;
    std::vector<Polygon> m_faces;
    std::vector<int> m_faceOfNumber;

    // --- Risorse GPU ---
    bool m_ready = false;
    int m_atlasCols = 5;
    int m_atlasRows = 4;
    Texture2D m_atlas{};
    RenderTexture2D m_rt{};
};

} // namespace ui