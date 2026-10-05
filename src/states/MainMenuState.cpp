#include "states/MainMenuState.h"
#include <memory>
#include "states/SettingsState.h"
#include "states/CharacterCreationState.h"
#include <iostream>
#include "core/Game.h"
#include <raylib.h>
MainMenuState::MainMenuState(class Game* game) : GameState(game), m_selectedOption(0) { m_type = GameStateType::MainMenu; }
void MainMenuState::Enter() { m_selectedOption = 0; }
void MainMenuState::Exit() {}
void MainMenuState::Update(float dt) {
    if (IsKeyPressed(KEY_DOWN)) { m_selectedOption = (m_selectedOption + 1) % 5; std::cout << "MENU ENTER selected=" << m_selectedOption << " action=NONE" << std::endl; }
    if (IsKeyPressed(KEY_UP)) { m_selectedOption = (m_selectedOption + 4) % 5; }
}
void MainMenuState::Render() { DrawText("MAIN MENU", 300, 150, 30, WHITE); const char* opts[] = {"Nuova partita","Continua","Carica","Impostazioni","Esci"}; for (int i=0;i<5;i++) DrawText(opts[i], 330, 200+i*30, 20, (i==m_selectedOption)?YELLOW:WHITE); }
void MainMenuState::HandleInput() {
    if (IsKeyPressed(KEY_ENTER)) {
        std::cout << "MENU ENTER selected=" << m_selectedOption << std::endl;
        if (m_selectedOption == 0) {
            std::cout << "STATE CHANGE: MainMenuState -> CharacterCreationState" << std::endl;
            if (m_game) { m_game->SetState(std::make_unique<CharacterCreationState>(m_game)); }
        }
        else if (m_selectedOption == 1) {
            std::cout << "MENU ENTER selected=1 action=CONTINUE -> Nessun salvataggio disponibile" << std::endl;
        }
        else if (m_selectedOption == 2) {
            std::cout << "MENU ENTER selected=2 action=LOAD -> Nessun salvataggio disponibile" << std::endl;
        }
        else if (m_selectedOption == 3) {
            std::cout << "STATE CHANGE: MainMenuState -> SettingsState" << std::endl;
            if (m_game) { m_game->SetState(std::make_unique<SettingsState>(m_game, SettingsState::FromMainMenu)); }
        }
        else if (m_selectedOption == 4) {
            std::cout << "MENU ENTER selected=4 action=EXIT" << std::endl;
            if (m_game) m_game->RequestExit();
        }
    }
}
