#pragma once
#include "states/GameState.h"

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
    int m_lastClickedOption = -1; // Minecraft-style: first click selects, second click activates
};
