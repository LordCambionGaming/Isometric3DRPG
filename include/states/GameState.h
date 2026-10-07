#pragma once

enum class GameStateType {
    Playing,
    Paused,
    MainMenu,
    Loading,
    DiceRoll,
    CharacterCreation,
    Inventory,
    VillageDialogue,
    VillageInterior
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
    Game* m_game = nullptr;
    GameStateType m_type = GameStateType::Playing;
};