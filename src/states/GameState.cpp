#include "states/GameState.h"

GameState::GameState(Game* game)
    : m_game(game)
    , m_type(GameStateType::Playing)
{
}