#include "core/GameState.h"
#include "core/Game.h"
#include <raylib.h>
#include "core/CombatSystem.h"

GameState::GameState(Game* game)
    : m_game(game)
    , m_type(GameStateType::Playing)
{
}

// ---------------------------------------------------------------------------
// PlayingState
//
// Tutta la logica di gioco vive in Game:
//   - Game::Update       -> aggiorna player, world, camera
//   - Game::Render       -> disegna world, player, hover tile e HUD (freccia)
//   - Game::HandleInput  -> picking del mouse e click-to-move
//
// PlayingState resta quindi volutamente sottile, per evitare di
// aggiornare/disegnare le stesse cose due volte per frame.
// ---------------------------------------------------------------------------
PlayingState::PlayingState(Game* game)
    : GameState(game)
    , m_showDebugInfo(true)
{
    m_type = GameStateType::Playing;
}

void PlayingState::Enter() {}

void PlayingState::Exit() {}

void PlayingState::Update(float /*deltaTime*/) {}

void PlayingState::Render() {}

void PlayingState::HandleInput() {
    if (IsKeyPressed(KEY_F3)) {
        m_showDebugInfo = !m_showDebugInfo;
    }

    // Tiro D20 durante Playing con tasto F5
    if (IsKeyPressed(KEY_F5) && m_game->GetCombatSystem()) {
        core::CombatRoll roll = m_game->GetCombatSystem()->TestCombatRoll(0);
        
        // Mostra risultato a schermo tramite debug info o HUD
        if (m_showDebugInfo) {
            std::cout << "\n=== COMBATTIMENTO D20 ===" << std::endl;
            std::cout << "Risultato: " << roll.rollValue << std::endl;
            
            switch (roll.result) {
                case core::CombatResult::CriticalFailure:
                    std::cout << "CRITICAL FAILURE! 1 naturale!" << std::endl;
                    break;
                case core::CombatResult::Normal:
                    std::cout << "Tiro normale." << std::endl;
                    break;
                case core::CombatResult::CriticalSuccess:
                    std::cout << "CRITICAL SUCCESS! 20 naturale!" << std::endl;
                    break;
            }
            std::cout << "Descrizione: " << roll.description << std::endl;
        }
    }
}
