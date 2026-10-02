#include "core/GameState.h"
#include "core/DiceRollState.h"
#include "core/Game.h"
#include <raylib.h>
#include <iostream>
#include <memory>

GameState::GameState(Game* game)
    : m_game(game)
    , m_type(GameStateType::Playing)
{
}

// ---------------------------------------------------------------------------
// PlayingState
//
// La logica di gioco vive in Game (update, render 3D, picking, click-to-move).
// PlayingState gestisce solo i tasti di gioco e l'overlay del risultato D20.
//   F3 : debug info
//   F5 : tiro di debug istantaneo
//   R  : apre la board del dado (DiceRollState, gioco in pausa)
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
    // Il testo del tiro di debug scompare dopo 2 secondi
    if (m_lastRoll.rollValue != 0) {
        m_rollTimer += deltaTime;
        if (m_rollTimer >= 2.0f) {
            m_lastRoll = core::CombatRoll{};
            m_rollTimer = 0.0f;
        }
    }
}

void PlayingState::Render() {
    if (m_lastRoll.rollValue == 0) return;

    const int fontSize = 48;
    const int cx = GetScreenWidth() / 2;
    const int cy = GetScreenHeight() / 2;

    Color textColor = WHITE;
    const char* resultText = "Tiro normale.";
    switch (m_lastRoll.result) {
        case core::CombatResult::CriticalFailure:
            textColor = RED;
            resultText = "CRITICAL FAILURE!";
            break;
        case core::CombatResult::CriticalSuccess:
            textColor = GREEN;
            resultText = "CRITICAL SUCCESS!";
            break;
        default:
            break;
    }

    const char* numText = TextFormat("D20: %d", m_lastRoll.rollValue);
    DrawText(numText, cx - MeasureText(numText, fontSize) / 2,
             cy - fontSize, fontSize, textColor);
    DrawText(resultText, cx - MeasureText(resultText, fontSize) / 2,
             cy + 10, fontSize, textColor);
}

void PlayingState::HandleInput() {
    if (IsKeyPressed(KEY_F3)) {
        m_showDebugInfo = !m_showDebugInfo;
    }

    // F5: tiro di debug istantaneo (TestCombatRoll stampa già in console)
    if (IsKeyPressed(KEY_F5) && m_game->GetCombatSystem()) {
        m_lastRoll = m_game->GetCombatSystem()->TestCombatRoll(0);
        m_rollTimer = 0.0f;
    }

    // R: apre la board del dado, il gioco va in pausa
    if (IsKeyPressed(KEY_R)) {
        m_game->SetState(std::make_unique<DiceRollState>(m_game));
        return;   // PlayingState è stato distrutto da SetState: non toccare i membri
    }
}