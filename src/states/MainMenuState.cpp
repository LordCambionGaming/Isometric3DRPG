#include "states/MainMenuState.h"
#include <memory>
#include "audio/AudioManager.h"
#include "states/SettingsState.h"
#include "states/CharacterCreationState.h"
#include "core/KeyBindings.h"
#include "core/Localization.h"
#include "core/Game.h"
#include "ui/MedievalMenu.h"
#include <raylib.h>

MainMenuState::MainMenuState(class Game* game) : GameState(game), m_selectedOption(0) { m_type = GameStateType::MainMenu; }
void MainMenuState::Enter() {
    m_selectedOption = 0;
    if (AudioManager* audio = m_game->GetAudioManager()) {
        audio->ClearPlaylist();
        audio->PlayMusic("music/MainTheme.mp3");
    }
}
void MainMenuState::Exit() {}
void MainMenuState::Update(float dt) {
    if (core::KeyBindings::Pressed(core::KeyBindings::Action::Down)) m_selectedOption = (m_selectedOption + 1) % 5;
    if (core::KeyBindings::Pressed(core::KeyBindings::Action::Up)) m_selectedOption = (m_selectedOption + 4) % 5;
}

void MainMenuState::Render() {
    const std::vector<ui::menu::MenuItem> items = {
        { lang::Get("menu.new_game"), "" },
        { lang::Get("menu.continue"), m_game && m_game->HasSave() ? "" : lang::Get("menu.no_save") },
        { lang::Get("menu.load_game"), m_game && m_game->HasSave() ? "" : lang::Get("menu.no_save") },
        { lang::Get("menu.settings"), "" },
        { lang::Get("menu.quit"), "" }
    };
    ui::menu::Draw(lang::Get("menu.title").c_str(), lang::Get("menu.subtitle").c_str(),
                   items, m_selectedOption, true);
}

void MainMenuState::HandleInput() {
    const int clickedOption = IsMouseButtonPressed(MOUSE_LEFT_BUTTON)
        ? ui::menu::OptionAt(GetMousePosition(), 5) : -1;
    const bool clicked = clickedOption >= 0;
    if (clicked) m_selectedOption = clickedOption;

    if (core::KeyBindings::Pressed(core::KeyBindings::Action::Back)) {
        if (m_game) m_game->RequestExit();
        return;
    }
    if (core::KeyBindings::Pressed(core::KeyBindings::Action::Confirm) || clicked) {
        if (m_selectedOption == 0) {
            if (m_game) { m_game->SetState(std::make_unique<CharacterCreationState>(m_game)); }
        }
        else if (m_selectedOption == 1 || m_selectedOption == 2) {
            if (m_game && m_game->HasSave()) m_game->LoadGame();
        }
        else if (m_selectedOption == 3) {
            if (m_game) { m_game->SetState(std::make_unique<SettingsState>(m_game, SettingsState::FromMainMenu)); }
        }
        else if (m_selectedOption == 4) {
            if (m_game) m_game->RequestExit();
        }
    }
}
