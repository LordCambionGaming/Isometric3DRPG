#pragma once
#include "states/GameState.h"
#include <string>

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
    int m_lastClickedOption = -1; // Minecraft-style: first click selects, second click activates
    float m_saveMessageTimer = 0.0f;
    std::string m_saveMessageKey;
};
