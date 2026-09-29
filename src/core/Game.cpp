#include "core/Game.h"
#include "core/GameState.h"
#include "core/Timer.h"
#include "rendering/Renderer.h"
#include "rendering/Camera3DController.h"
#include "world/World.h"
#include "assets/AssetManager.h"
#include "audio/AudioManager.h"
#include <raylib.h>
#include <iostream>

Game::Game()
    : m_isRunning(false)
    , m_debugMode(false)
    , m_gameState(nullptr)
    , m_renderer(nullptr)
    , m_camera(nullptr)
    , m_world(nullptr)
    , m_assetManager(nullptr)
    , m_audioManager(nullptr)
    , m_timer(nullptr)
{
}

Game::~Game() {
    Shutdown();
}

bool Game::Initialize() {
    try {
        // Initialize core systems
        m_timer = std::make_unique<Timer>();
        m_assetManager = std::make_unique<AssetManager>();
        m_audioManager = std::make_unique<AudioManager>();
        m_renderer = std::make_unique<Renderer>();
        m_camera = std::make_unique<Camera3DController>();
        m_world = std::make_unique<World>();

        // Initialize renderer (creates window)
        if (!m_renderer->Initialize(SCREEN_WIDTH, SCREEN_HEIGHT, WINDOW_TITLE)) {
            std::cerr << "Failed to initialize renderer!" << std::endl;
            return false;
        }

        // Initialize other systems
        if (!m_assetManager->Initialize()) {
            std::cerr << "Failed to initialize asset manager!" << std::endl;
            return false;
        }

        if (!m_audioManager->Initialize()) {
            std::cerr << "Failed to initialize audio manager!" << std::endl;
            return false;
        }

        // Initialize camera
        m_camera->Initialize();
        m_camera->SetIsometricView();

        // Initialize world
        WorldConfig worldConfig;
        worldConfig.seed = 12345;
        worldConfig.chunkSize = 16;
        worldConfig.tileSize = 1.0f;
        worldConfig.renderDistance = 3;

        if (!m_world->Initialize(worldConfig)) {
            std::cerr << "Failed to initialize world!" << std::endl;
            return false;
        }

        // Connect systems
        m_renderer->SetWorld(m_world.get());
        m_renderer->SetCamera(m_camera.get());

        // Create initial game state
        m_gameState = std::make_unique<PlayingState>(this);
        m_gameState->Enter();

        m_isRunning = true;
        m_debugMode = true; // Start with debug info visible

        std::cout << "Game initialized successfully!" << std::endl;
        std::cout << "World seed: " << worldConfig.seed << std::endl;

        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception during initialization: " << e.what() << std::endl;
        return false;
    }
}

void Game::Run() {
    while (m_isRunning && !m_renderer->ShouldClose()) {
        m_timer->Update();
        float deltaTime = m_timer->GetDeltaTime();

        HandleInput();
        Update(deltaTime);
        Render();
    }
}

void Game::Shutdown() {
    if (m_gameState) {
        m_gameState->Exit();
        m_gameState.reset();
    }

    if (m_world) m_world.reset();
    if (m_audioManager) m_audioManager.reset();
    if (m_assetManager) m_assetManager.reset();
    if (m_camera) m_camera.reset();
    if (m_renderer) m_renderer.reset();
    if (m_timer) m_timer.reset();

    m_isRunning = false;
}

void Game::Update(float deltaTime) {
    if (m_gameState) {
        m_gameState->Update(deltaTime);
    }

    if (m_camera) {
        m_camera->Update(deltaTime);
    }

    if (m_world) {
        m_world->Update(deltaTime);
    }

    if (m_audioManager) {
        m_audioManager->UpdateMusic();
    }
}

void Game::Render() {
    m_renderer->BeginFrame();

    if (m_gameState) {
        m_gameState->Render();
    }

    if (m_debugMode) {
        m_renderer->RenderDebugInfo();
    }

    m_renderer->EndFrame();
}

void Game::HandleInput() {
    // Global input handling
    if (IsKeyPressed(KEY_F1)) {
        m_debugMode = !m_debugMode;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        m_isRunning = false;
    }

    // Delegate to current game state
    if (m_gameState) {
        m_gameState->HandleInput();
    }

    // Camera input
    if (m_camera) {
        m_camera->HandleInput();
    }
}