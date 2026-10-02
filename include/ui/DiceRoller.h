#pragma once
#include <functional>
#include <raylib.h>
#include "core/CombatSystem.h"

namespace ui {

class DiceRoller {
public:
    enum class State { Idle, Dragging, Flying, Settled };
    using RollProvider = std::function<core::CombatRoll()>;

    void SetRollProvider(RollProvider provider) { m_provider = std::move(provider); }
    void SetBounds(Rectangle bounds) { m_bounds = bounds; }  // area in cui il dado può muoversi
    void Reset();

    void HandleInput();
    void Update(float dt);
    void Render() const;

    State GetState() const { return m_state; }
    bool IsBusy() const { return m_state == State::Dragging || m_state == State::Flying; }

    // true una sola volta, quando il dado si ferma
    bool ConsumeResult(core::CombatRoll& out);

private:
    Vector2 RestPos() const;

    State m_state = State::Idle;
    Rectangle m_bounds{0, 0, 600, 400};
    Vector2 m_pos{0, 0};
    Vector2 m_vel{0, 0};
    Vector2 m_grab{0, 0};
    float m_rot = 0.0f;
    float m_spin = 0.0f;
    float m_tick = 0.0f;
    int   m_shown = 20;

    core::CombatRoll m_pending{};
    bool m_hasResult = false;
    RollProvider m_provider;
};

} // namespace ui