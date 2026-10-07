#include "states/PauseState.h"
#include <memory>
#include <iostream>
#include <algorithm>
#include "states/SettingsState.h"
#include "states/MainMenuState.h"
#include "core/KeyBindings.h"
#include "core/Localization.h"
#include "core/Game.h"
#include "ui/MedievalMenu.h"
#include <raylib.h>
#include <states/PlayingState.h>

PauseState::PauseState(class Game* game) : GameState(game), m_selectedOption(0) { m_type = GameStateType::Paused; }
void PauseState::Enter() {}
void PauseState::Exit() {}
void PauseState::Update(float dt) {
    if (m_saveMessageTimer > 0.0f) {
        m_saveMessageTimer = std::max(0.0f, m_saveMessageTimer - dt);
        if (m_saveMessageTimer == 0.0f) m_saveMessageKey.clear();
    }
    if (core::KeyBindings::Pressed(core::KeyBindings::Action::Down)) m_selectedOption = (m_selectedOption + 1) % 5;
    if (core::KeyBindings::Pressed(core::KeyBindings::Action::Up)) m_selectedOption = (m_selectedOption + 4) % 5;
}

void PauseState::Render() {
    const std::string saveMessage = m_saveMessageKey.empty() ? std::string{} : lang::Get(m_saveMessageKey);
    const std::vector<ui::menu::MenuItem> items = {
        { lang::Get("pause.resume"), "" },
        { lang::Get("pause.save"), saveMessage },
        { lang::Get("menu.settings"), "" },
        { lang::Get("pause.return_menu"), "" },
        { lang::Get("menu.quit"), "" }
    };
    ui::menu::Draw(lang::Get("pause.title").c_str(), lang::Get("pause.subtitle").c_str(), items, m_selectedOption);
}

void PauseState::HandleInput() {
    const int clickedOption = IsMouseButtonPressed(MOUSE_LEFT_BUTTON)
        ? ui::menu::OptionAt(GetMousePosition(), 5) : -1;
    const bool clicked = clickedOption >= 0;
    if (clicked) m_selectedOption = clickedOption;
    if (core::KeyBindings::Pressed(core::KeyBindings::Action::Back)) m_selectedOption = 0;
    else if (!core::KeyBindings::Pressed(core::KeyBindings::Action::Confirm) && !clicked) return;

    if (m_selectedOption == 0) {
        if (m_game) m_game->SetState(std::make_unique<PlayingState>(m_game));
    }
    else if (m_selectedOption == 1) {
        const bool saved = m_game && m_game->SaveGame();
        m_saveMessageKey = saved ? "pause.saved" : "pause.save_failed";
        m_saveMessageTimer = 3.0f;
    }
    else if (m_selectedOption == 2) { if (m_game) m_game->SetState(std::make_unique<SettingsState>(m_game, SettingsState::FromPause)); }
    else if (m_selectedOption == 3) {
        if (m_game) {
            m_game->ResetGameSession();
            m_game->SetState(std::make_unique<MainMenuState>(m_game));
        }
    }
    else if (m_selectedOption == 4) { if (m_game) m_game->RequestExit(); }
}
