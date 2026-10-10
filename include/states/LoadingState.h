#pragma once

#include "states/GameState.h"

class LoadingState : public GameState {
public:
    explicit LoadingState(Game* game);

    void Enter() override;
    void Exit() override;
    void Update(float deltaTime) override;
    void Render() override;
    void HandleInput() override;

private:
    bool m_skipFirstUpdate = true;
};
