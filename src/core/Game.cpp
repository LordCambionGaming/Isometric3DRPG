#include "core/Game.h"
#include "core/GameState.h"
#include "core/PauseState.h"
#include "core/MainMenuState.h"
#include "player/Player.h"
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

Player* g_player = nullptr;  // player globale temporaneo per integrazione
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
        m_gameState = std::make_unique<MainMenuState>(this);
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

void Game::SetState(std::unique_ptr<GameState> state) {
    std::cout << "[STATE CHANGE] SetState called" << std::endl;
    if (m_gameState) { m_gameState->Exit(); }
    m_gameState = std::move(state);
    m_stateChangedThisFrame = true;
    if (m_gameState) m_gameState->Enter();
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
    m_stateChangedThisFrame = false;

    // Player update (integrato dopo ESC)
    static Player* g_player = nullptr; if (!g_player) g_player = new Player(); g_player->Update(deltaTime);
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

    // Null-check before dereferencing
    if (m_gameState) {
        std::cout << "ESC pressed, current type=" << (int)m_gameState->GetType() << std::endl;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        std::cout << "[ESC] pressed" << std::endl;
        std::cout << "[GAME] HandleInput KEY_ESCAPE" << std::endl;
        if (m_gameState && m_gameState->GetType() == GameStateType::Playing) {
            m_gameState = std::make_unique<PauseState>(this);
            std::cout << "[STATE] Playing -> Pause" << std::endl;
            m_stateChangedThisFrame = true;
            if (m_gameState) { m_gameState->Enter(); }
        } else if (m_gameState && m_gameState->GetType() == GameStateType::MainMenu) {
            m_isRunning = false;
        } else if (m_gameState && m_gameState->GetType() == GameStateType::Paused) {
            // Da Pause: resta in Pause; back via ENTER opzione 3
        }
    }

    // Delegate to current game state only if no transition occurred this frame
    if (m_gameState && !m_stateChangedThisFrame) {
        m_gameState->HandleInput();
    }

    // Camera input
    if (m_camera) {
        m_camera->HandleInput();
    }
}