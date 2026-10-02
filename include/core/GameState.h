#pragma once
#include "core/CombatSystem.h"
enum class GameStateType {
    Playing,
    Paused,
    MainMenu,
    Loading
};

class Game;

class GameState {
public:
    explicit GameState(Game* game);
    virtual ~GameState() = default;

    virtual void Enter() = 0;
    virtual void Exit() = 0;
    virtual void Update(float deltaTime) = 0;
    virtual void Render() = 0;
    virtual void HandleInput() = 0;

    GameStateType GetType() const { return m_type; }

protected:
    Game* m_game;
    GameStateType m_type;
};

class PlayingState : public GameState {
public:
    explicit PlayingState(Game* game);

    void Enter() override;
    void Exit() override;
    void Update(float deltaTime) override;
    void Render() override;
    void HandleInput() override;

private:
    bool m_showDebugInfo;
    core::CombatRoll m_lastRoll;
    float m_rollTimer = 0.0f;
};
