#pragma once
#include "states/GameState.h"
#include "core/CombatSystem.h"

class PlayingState : public GameState {
public:
    explicit PlayingState(Game* game);
    ~PlayingState() override = default;

    void Enter() override;
    void Exit() override;
    void Update(float deltaTime) override;
    void Render() override;
    void HandleInput() override;

    bool IsDebugInfoVisible() const { return m_showDebugInfo; }

private:
    bool m_showDebugInfo = true;
    core::CombatRoll m_lastRoll{};
    float m_rollTimer = 0.0f;
};