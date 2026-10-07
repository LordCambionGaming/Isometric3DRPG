#pragma once

#include "states/GameState.h"
#include <raylib.h>

class VillageInteriorState : public GameState {
public:
    VillageInteriorState(Game* game, int buildingIndex);

    void Enter() override;
    void Exit() override;
    void Update(float deltaTime) override;
    void Render() override;
    void HandleInput() override;

private:
    int m_buildingIndex;
    Vector3 m_moveTarget{};
    bool m_moving = false;
};
