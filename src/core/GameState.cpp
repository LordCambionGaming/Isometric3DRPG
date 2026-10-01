#include "core/GameState.h"
#include "core/Game.h"
#include <raylib.h>

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
}