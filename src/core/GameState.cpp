#include "core/GameState.h"
#include "core/Game.h"
#include <raylib.h>
#include "core/CombatSystem.h"
#include <iostream>

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

void PlayingState::Update(float deltaTime) {
    // Timer per far scomparire il testo del tiro D20 dopo 2 secondi
    if (m_lastRoll.rollValue != 0) {
        m_rollTimer += deltaTime;
        if (m_rollTimer >= 2.0f) {
            m_lastRoll = core::CombatRoll{0, core::CombatResult::Normal, ""};
            m_rollTimer = 0.0f;
        }
    }
}

void PlayingState::Render() {
    // Mostra risultato D20 a schermo se attivo
    if (m_lastRoll.rollValue != 0) {
        const float fontSize = 48.0f;
        const float textPadding = 16.0f;
        
        // Posizione centro schermo con offset Y per leggibilità
        const Vector2 pos = { GetScreenWidth() / 2.0f - textPadding, 
                             GetScreenHeight() / 2.0f + 50.0f };
        
        // Colore in base al risultato
        Color textColor;
        switch (m_lastRoll.result) {
            case core::CombatResult::CriticalFailure:
                textColor = RED;
                break;
            case core::CombatResult::CriticalSuccess:
                textColor = GREEN;
                break;
            default:
                textColor = WHITE;
                break;
        }
        
        // Disegna numero del tiro
        DrawText(TextFormat("D20: %d", m_lastRoll.rollValue), pos.x, pos.y - fontSize * 1.5f, 
                 fontSize, textColor);
        
        // Disegna descrizione risultato
        const char* resultText = "";
        switch (m_lastRoll.result) {
            case core::CombatResult::CriticalFailure:
                resultText = "CRITICAL FAILURE!";
                break;
            case core::CombatResult::Normal:
                resultText = "Tiro normale.";
                break;
            case core::CombatResult::CriticalSuccess:
                resultText = "CRITICAL SUCCESS!";
                break;
        }
        
        DrawText(resultText, pos.x, pos.y, fontSize, textColor);
    }
}

void PlayingState::HandleInput() {
    if (IsKeyPressed(KEY_F3)) {
        m_showDebugInfo = !m_showDebugInfo;
    }

    // Tiro D20 durante Playing con tasto F5
    if (IsKeyPressed(KEY_F5) && m_game->GetCombatSystem()) {
        core::CombatRoll roll = m_game->GetCombatSystem()->TestCombatRoll(0);
        
        // Salva risultato per visualizzazione grafica
        m_lastRoll = roll;
        m_rollTimer = 0.0f;
        
        // Mostra anche nella console (opzionale)
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
