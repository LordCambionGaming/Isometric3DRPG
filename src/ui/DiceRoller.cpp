#include "ui/DiceRoller.h"
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace ui {

namespace {
constexpr float kRadius        = 38.0f;
constexpr float kMinThrowSpeed = 350.0f;
constexpr float kMaxThrowSpeed = 2600.0f;
constexpr float kFriction      = 2.2f;
constexpr float kBounce        = 0.70f;
constexpr float kStopSpeed     = 60.0f;
}

Vector2 DiceRoller::RestPos() const {
    return { m_bounds.x + m_bounds.width / 2.0f,
             m_bounds.y + m_bounds.height - kRadius - 20.0f };
}

void DiceRoller::Reset() {
    m_state = State::Idle;
    m_pos = RestPos();
    m_vel = { 0, 0 };
    m_spin = 0.0f;
    m_shown = 20;
    m_hasResult = false;
}

void DiceRoller::HandleInput() {
    const Vector2 mouse = GetMousePosition();
    const float dt = std::max(GetFrameTime(), 0.0001f);

    if (m_state == State::Idle || m_state == State::Settled) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
            Vector2Distance(mouse, m_pos) <= kRadius) {
            m_state = State::Dragging;
            m_grab = Vector2Subtract(m_pos, mouse);
            m_vel = { 0, 0 };
        }
    }
    else if (m_state == State::Dragging) {
        Vector2 target = Vector2Add(mouse, m_grab);
        target.x = Clamp(target.x, m_bounds.x + kRadius, m_bounds.x + m_bounds.width  - kRadius);
        target.y = Clamp(target.y, m_bounds.y + kRadius, m_bounds.y + m_bounds.height - kRadius);

        const Vector2 instVel = Vector2Scale(Vector2Subtract(target, m_pos), 1.0f / dt);
        m_vel = Vector2Lerp(m_vel, instVel, 0.35f);
        m_pos = target;

        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            float speed = Vector2Length(m_vel);
            if (speed >= kMinThrowSpeed) {
                if (speed > kMaxThrowSpeed) {
                    m_vel = Vector2Scale(m_vel, kMaxThrowSpeed / speed);
                    speed = kMaxThrowSpeed;
                }
                m_spin = speed * 0.4f * (m_vel.x >= 0 ? 1.0f : -1.0f);
                if (m_provider) m_pending = m_provider();   // tiro VERO deciso qui
                m_state = State::Flying;
            } else {
                m_state = State::Idle;                       // lancio troppo debole
            }
        }
    }
}

void DiceRoller::Update(float dt) {
    switch (m_state) {
    case State::Idle:
        m_pos = Vector2Lerp(m_pos, RestPos(), std::min(1.0f, dt * 10.0f));
        m_rot += 20.0f * dt;
        break;

    case State::Dragging:
        m_rot += 90.0f * dt;
        break;

    case State::Flying: {
        m_pos = Vector2Add(m_pos, Vector2Scale(m_vel, dt));
        const float damp = std::max(0.0f, 1.0f - kFriction * dt);
        m_vel = Vector2Scale(m_vel, damp);
        m_spin *= damp;
        m_rot += m_spin * dt;

        const float l = m_bounds.x + kRadius,  r = m_bounds.x + m_bounds.width  - kRadius;
        const float t = m_bounds.y + kRadius,  b = m_bounds.y + m_bounds.height - kRadius;
        if (m_pos.x < l) { m_pos.x = l; m_vel.x = -m_vel.x * kBounce; }
        if (m_pos.x > r) { m_pos.x = r; m_vel.x = -m_vel.x * kBounce; }
        if (m_pos.y < t) { m_pos.y = t; m_vel.y = -m_vel.y * kBounce; }
        if (m_pos.y > b) { m_pos.y = b; m_vel.y = -m_vel.y * kBounce; }

        m_tick -= dt;
        if (m_tick <= 0.0f) { m_shown = GetRandomValue(1, 20); m_tick = 0.06f; }

        if (Vector2Length(m_vel) < kStopSpeed) {
            m_state = State::Settled;
            m_shown = m_pending.rollValue;
            m_hasResult = true;
        }
        break;
    }

    case State::Settled:
        break;   // resta fermo finché non lo riprendi
    }
}

bool DiceRoller::ConsumeResult(core::CombatRoll& out) {
    if (!m_hasResult) return false;
    out = m_pending;
    m_hasResult = false;
    return true;
}

void DiceRoller::Render() const {
    // vaschetta di partenza
    const Vector2 rest = RestPos();
    DrawCircleV(rest, kRadius + 14.0f, Fade(BLACK, 0.35f));
    DrawCircleLines((int)rest.x, (int)rest.y, kRadius + 14.0f, Fade(WHITE, 0.4f));

    float scale = 1.0f;
    if (m_state == State::Flying)
        scale += 0.3f * std::min(Vector2Length(m_vel) / 1500.0f, 1.0f);
    else if (m_state == State::Dragging)
        scale = 1.15f;
    const float r = kRadius * scale;

    DrawCircleV({ m_pos.x + 6.0f, m_pos.y + 8.0f }, r * 0.95f, Fade(BLACK, 0.35f));

    Color fill = { 70, 90, 200, 255 };
    if (m_state == State::Settled) {
        if (m_pending.result == core::CombatResult::CriticalSuccess)      fill = { 40, 170, 70, 255 };
        else if (m_pending.result == core::CombatResult::CriticalFailure) fill = { 190, 50, 50, 255 };
    }

    DrawPoly(m_pos, 6, r, m_rot, fill);
    DrawPoly(m_pos, 3, r * 0.62f, m_rot + 30.0f, Fade(WHITE, 0.15f));
    DrawPolyLinesEx(m_pos, 6, r, m_rot, 3.0f, WHITE);

    const char* txt = TextFormat("%d", m_shown);
    const int fs = 28;
    DrawText(txt, (int)m_pos.x - MeasureText(txt, fs) / 2, (int)m_pos.y - fs / 2, fs, WHITE);
}

} // namespace ui