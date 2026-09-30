#pragma once
#include "core/GameState.h"

class MainMenuState : public GameState {
public:
    explicit MainMenuState(class Game* game);
    void Enter() override;
    void Exit() override;
    void Update(float deltaTime) override;
    void Render() override;
    void HandleInput() override;
private:
    int m_selectedOption;
};
