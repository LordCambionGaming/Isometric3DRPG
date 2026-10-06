#pragma once
#include "states/GameState.h"

class InventoryState : public GameState {
public:
    explicit InventoryState(Game* game);
    void Enter() override {}
    void Exit() override {}
    void Update(float) override {}
    void Render() override;
    void HandleInput() override;
private:
    int m_selected = 0;
    void Close();
};