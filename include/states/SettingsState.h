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
    enum class Page { General, Controls };

    PreviousState m_prev;
    Page m_page = Page::General;
    int m_selectedOption = 0;
    int m_lastClickedOption = -1; // Minecraft-style: first click selects, second click activates
    bool m_waitingForKey = false;
    int m_draggingSlider = -1;

    void ReturnToPreviousState();
    void ApplyGeneralSetting(int direction);
};
