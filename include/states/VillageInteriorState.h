#pragma once

#include "states/GameState.h"
#include <raylib.h>
#include <vector>

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
    std::vector<Vector3> m_path;
    std::size_t m_pathIndex = 0;
    bool m_moving = false;

    void SetMoveTarget(float x, float z);
};
