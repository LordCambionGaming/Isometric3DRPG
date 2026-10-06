#pragma once
#include <functional>
#include <raylib.h>
#include <raymath.h>
#include "core/CombatSystem.h"
#include "ui/DiceGeometry.h"

namespace ui {

// D20 3D (icosaedro) con lancio trascinabile.
// Fisica 2D per la posizione, orientamento 3D con quaternioni.
// Il risultato vero viene deciso al lancio; la faccia finale lo mostra.
class DiceRoller {
public:
    enum class State { Idle, Dragging, Flying, Settled };
    using RollProvider = std::function<core::CombatRoll()>;

    DiceRoller();
    ~DiceRoller();
    DiceRoller(const DiceRoller&) = delete;
    DiceRoller& operator=(const DiceRoller&) = delete;

    void SetRollProvider(RollProvider provider) { m_provider = std::move(provider); }
    void SetBounds(Rectangle bounds) { m_bounds = bounds; }
    void Reset();

    // Quale dado e' sul feltro: 4, 6, 8, 10, 12 o 20 (default 20). Chiamala prima del lancio.
    // Il valore restituito dal RollProvider deve stare in 1..sides.
    void SetSides(int sides);
    int GetSides() const { return m_sides; }

    void HandleInput();
    void Update(float dt);
    void Render();                       // non const: disegna in una RenderTexture

    State GetState() const { return m_state; }
    bool IsBusy() const { return m_state == State::Dragging || m_state == State::Flying; }

    // true una sola volta, quando il dado si e' fermato E allineato sulla faccia
    bool ConsumeResult(core::CombatRoll& out);

private:
    void BuildGeometry();
    void BuildAtlas();
    void EnsureResources();
    void DrawDie3D();
    void Integrate(float dt);
    Quaternion TargetFor(int face) const;
    Vector2 RestPos() const;

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

    // geometria (vedi DiceGeometry.h)
    int       m_sides = 20;
    DiceShape m_shape;

    // risorse GPU (create al primo Render)
    bool m_ready = false;
    Texture2D m_atlas{};
    RenderTexture2D m_rt{};
};

} // namespace ui