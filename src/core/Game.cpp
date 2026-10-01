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
#include <cmath>

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

        m_player = std::make_unique<Player>();
        m_player->SetTileSize(worldConfig.tileSize);
        float initHeight = m_world->GetHeightAt(0.0f, 0.0f);
        m_player->SetPosition({0.5f * worldConfig.tileSize, initHeight, 0.5f * worldConfig.tileSize});

        m_navigation.Initialize(m_world.get(), MAX_STEP_HEIGHT);

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
    if (m_player) {
        m_player->Update(deltaTime);
    }
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

    // Render world with 3D mode (only during Playing state)
    if (m_gameState &&
        m_gameState->GetType() == GameStateType::Playing &&
        m_camera &&
        m_world)
    {
        Camera3D cam = m_camera->GetCamera();
        m_renderer->BeginMode3D(cam);
        m_world->Render();
        if (m_player) m_player->Render();

        // Hover tile outline (YELLOW line outline) — on actual tile surface
        if (m_hoverTileX != -9999 && m_hoverTileZ != -9999) {
            int tx = m_hoverTileX;
            int tz = m_hoverTileZ;

            // Get terrain height at tile center
            float tileCenterX = tx + 0.5f;
            float tileCenterZ = tz + 0.5f;
            float tileHeight = m_world->GetHeightAt(tileCenterX, tileCenterZ);
            float surfaceY = tileHeight + 0.05f; // small offset to avoid z-fighting

            float s = 1.0f; // full tile size
            Vector3 p0 = { (float)tx, surfaceY, (float)tz };
            Vector3 p1 = { (float)(tx + s), surfaceY, (float)tz };
            Vector3 p2 = { (float)(tx + s), surfaceY, (float)(tz + s) };
            Vector3 p3 = { (float)tx, surfaceY, (float)(tz + s) };
            DrawLine3D(p0, p1, YELLOW);
            DrawLine3D(p1, p2, YELLOW);
            DrawLine3D(p2, p3, YELLOW);
            DrawLine3D(p3, p0, YELLOW);
        }

        m_renderer->EndMode3D();
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

    // Click-to-move + hover tile calculation (only when world available and playing)
    if (m_camera && m_world && m_player && m_gameState && m_gameState->GetType() == GameStateType::Playing) {
        Ray ray = GetMouseRay(GetMousePosition(), m_camera->GetCamera());

        // 3D voxel raycast picking: test ray against terrain geometry
        float maxDist = 100.0f;
        float step = 0.5f;
        int bestTileX = -9999;
        int bestTileZ = -9999;
        float bestDist = maxDist;
        
        for (float d = 0.1f; d < maxDist; d += step) {
            Vector3 point = {
                ray.position.x + ray.direction.x * d,
                ray.position.y + ray.direction.y * d,
                ray.position.z + ray.direction.z * d
            };
            int tx = (int)std::floor(point.x);
            int tz = (int)std::floor(point.z);
            float tileCenterX = tx + 0.5f;
            float tileCenterZ = tz + 0.5f;
            float tileHeight = m_world->GetHeightAt(tileCenterX, tileCenterZ);
            float surfaceY = tileHeight;
            
            if (point.x >= (float)tx && point.x <= (float)(tx + 1.0f) &&
                point.z >= (float)tz && point.z <= (float)(tz + 1.0f) &&
                point.y >= 0.0f && point.y <= surfaceY) {
                bestTileX = tx;
                bestTileZ = tz;
                bestDist = d;
                break;
            }
        }
        
        if (bestTileX != -9999 && bestTileZ != -9999) {
            m_hoverTileX = bestTileX;
            m_hoverTileZ = bestTileZ;
            float tileCenterX = bestTileX + 0.5f;
            float tileCenterZ = bestTileZ + 0.5f;
            float tileHeight = m_world->GetHeightAt(tileCenterX, tileCenterZ);
            float surfaceY = tileHeight;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (m_player) {
                    int curTileX = m_player->GetTileX();
                    int curTileZ = m_player->GetTileZ();

                    // Use NavigationSystem to find path
                    std::vector<TileCoord> path = m_navigation.FindPath(curTileX, curTileZ, bestTileX, bestTileZ);

                    if (!path.empty()) {
                        std::cout << "[CLICK] Path found with " << path.size() << " tiles from ("
                                  << curTileX << "," << curTileZ << ") to ("
                                  << bestTileX << "," << bestTileZ << ")" << std::endl;
                        m_player->SetPath(path);
                    } else {
                        std::cout << "[CLICK] No path to tile (" << bestTileX << "," << bestTileZ << ")" << std::endl;
                    }
                }
            }
        } else {
            m_hoverTileX = -9999;
            m_hoverTileZ = -9999;
        }
    }
}