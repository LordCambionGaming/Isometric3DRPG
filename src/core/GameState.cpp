#include "core/GameState.h"
#include "core/Game.h"
#include "rendering/Renderer.h"
#include "rendering/Camera3DController.h"
#include "world/World.h"
#include "core/Timer.h"
#include <raylib.h>
#include <iostream>

GameState::GameState(Game* game)
    : m_game(game)
    , m_type(GameStateType::Playing)
{
}

// Playing State Implementation
PlayingState::PlayingState(Game* game)
    : GameState(game)
    , m_showDebugInfo(true)
{
    m_type = GameStateType::Playing;
}

void PlayingState::Enter() {
    std::cout << "Entering Playing state" << std::endl;
}

void PlayingState::Exit() {
    std::cout << "Exiting Playing state" << std::endl;
}

void PlayingState::Update(float deltaTime) {
    // Update world based on camera position for chunk loading
    if (auto* world = m_game->GetWorld()) {
        auto* camera = m_game->GetCamera();
        if (camera) {
            Vector3 cameraPos = camera->GetPosition();
            world->Update(deltaTime);
        }
    }
}

void PlayingState::Render() {
    auto* renderer = m_game->GetRenderer();
    auto* camera = m_game->GetCamera();
    auto* world = m_game->GetWorld();

    if (!renderer || !camera) return;

    // Begin 3D rendering
    renderer->BeginMode3D(camera->GetCamera());

    // Render world
    if (world) {
        world->Render();
    }

    // Render coordinate system for debugging
    renderer->EndMode3D();

    // Render UI
    renderer->RenderUI();
}

void PlayingState::HandleInput() {
    // Toggle debug info
    if (IsKeyPressed(KEY_F3)) {
        m_showDebugInfo = !m_showDebugInfo;
    }

    // Camera controls are handled in Camera3DController
    // World generation controls
    if (IsKeyPressed(KEY_R)) {
        if (auto* world = m_game->GetWorld()) {
            // Regenerate world with new seed
            unsigned int newSeed = GetRandomValue(1, 999999);
            world->SetSeed(newSeed);
            std::cout << "New world generated with seed: " << newSeed << std::endl;
        }
    }
}