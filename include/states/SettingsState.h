#pragma once
#include "states/GameState.h"
class SettingsState : public GameState {
public:
    enum PreviousState { FromPause, FromMainMenu };
    explicit SettingsState(class Game* game, PreviousState prev = FromMainMenu);
    PreviousState GetPreviousState() const { return m_prev; }
    void Enter() override; void Exit() override;
    void Update(float deltaTime) override; void Render() override; void HandleInput() override;
private:
    PreviousState m_prev;
};
