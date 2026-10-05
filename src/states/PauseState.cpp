#include "states/PauseState.h"
#include <memory>
#include <iostream>
#include "states/SettingsState.h"
#include "states/MainMenuState.h"
#include "core/Game.h"
#include <raylib.h>
#include <states/PlayingState.h>
PauseState::PauseState(class Game* game) : GameState(game), m_selectedOption(0) { m_type = GameStateType::Paused; }
void PauseState::Enter() {}
void PauseState::Exit() {}
void PauseState::Update(float dt) { if (IsKeyPressed(KEY_DOWN)) m_selectedOption = (m_selectedOption+1)%5; if (IsKeyPressed(KEY_UP)) m_selectedOption = (m_selectedOption+4)%5; }
void PauseState::Render() { DrawText("PAUSE", 350, 150, 30, WHITE); const char* opts[] = {"Riprendi","Salva","Impostazioni","Torna al menu","Esci"}; for (int i=0; i<5; i++) DrawText(opts[i], 330, 200+i*30, 20, (i==m_selectedOption)?YELLOW:WHITE); }
void PauseState::HandleInput() {
    if (IsKeyPressed(KEY_ENTER) && m_selectedOption==0) {
        std::cout << "STATE CHANGE: PauseState -> PlayState" << std::endl;
        if (m_game) m_game->SetState(std::make_unique<PlayingState>(m_game));
    }
    else if (IsKeyPressed(KEY_ENTER) && m_selectedOption==1) { std::cout << "PAUSE SAVE -> not implemented" << std::endl; }
    else if (IsKeyPressed(KEY_ENTER) && m_selectedOption==2) { std::cout << "PAUSE SETTINGS -> SettingsState" << std::endl; if (m_game) m_game->SetState(std::make_unique<SettingsState>(m_game, SettingsState::FromPause)); }
    else if (IsKeyPressed(KEY_ENTER) && m_selectedOption==3) { std::cout << "[STATE CHANGE] Pause -> MainMenu (Reset session)" << std::endl; if (m_game) { m_game->ResetGameSession(); m_game->SetState(std::make_unique<MainMenuState>(m_game)); } }
    else if (IsKeyPressed(KEY_ENTER) && m_selectedOption==4) { if (m_game) m_game->RequestExit(); }
}
