#pragma once
#include "states/GameState.h"

class PauseState : public GameState {
public:
    explicit PauseState(class Game* game);
    void Enter() override;
    void Exit() override;
    void Update(float deltaTime) override;
    void Render() override;
    void HandleInput() override;
private:
    int m_selectedOption;
};
