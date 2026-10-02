#pragma once
#include "core/GameState.h"
#include "ui/DiceRoller.h"

class DiceRollState : public GameState {
public:
    explicit DiceRollState(Game* game);

    void Enter() override;
    void Exit() override {}
    void Update(float deltaTime) override;
    void Render() override;
    void HandleInput() override;

private:
    Rectangle Board() const;

    ui::DiceRoller m_dice;
    core::CombatRoll m_result{};
};