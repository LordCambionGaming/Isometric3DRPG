#include "core/Game.h"
#include "navigation/NavigationSystem.h"
#include "core/GameState.h"
#include "core/PauseState.h"
#include "core/MainMenuState.h"
#include "player/Player.h"
#include "core/Timer.h"
#include "rendering/Renderer.h"
#include "rendering/Camera3DController.h"
#include "world/World.h"
#include "world/Chunk.h"
#include "assets/AssetManager.h"
#include "audio/AudioManager.h"
#include "ui/HUD.h"
#include <raylib.h>
#include <raymath.h>
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
    , m_hud(nullptr)
    , m_combatSystem(nullptr)
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
        m_hud = std::make_unique<HUD>();
        m_combatSystem = std::make_unique<CombatSystem>();

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

        // Deterministic spawn: find valid land tile near origin (0,0)
        // Valid = height >= 2 (SEA_LEVEL) && !hasWater
        float spawnX = 0.5f * worldConfig.tileSize;
        float spawnZ = 0.5f * worldConfig.tileSize;
        float spawnY = 0.0f;
        bool found = false;
        int foundTx = 0, foundTz = 0;
        float foundHeight = 0.0f;
        const int MAX_SPAWN_SEARCH_RADIUS = 64;

        // Pre-carica solo i chunk attorno all'origine; gli altri vengono caricati on-demand
        const int preRadius = 1;
        for (int cx = -preRadius; cx <= preRadius; ++cx) {
            for (int cz = -preRadius; cz <= preRadius; ++cz) {
                if (!m_world->GetChunk(cx, cz)) m_world->LoadChunkAt(cx, cz);
            }
        }

        // Ricerca deterministica per anelli (raggio crescente)
        for (int r = 0; r <= MAX_SPAWN_SEARCH_RADIUS && !found; ++r) {
            for (int dx = -r; dx <= r && !found; ++dx) {
                for (int dz = -r; dz <= r && !found; ++dz) {
                    if (r > 0 && std::abs(dx) != r && std::abs(dz) != r) continue; // solo l'anello
                    const int tx = dx;
                    const int tz = dz;

                    const int cx = (tx >= 0) ? (tx / worldConfig.chunkSize) : ((tx - worldConfig.chunkSize + 1) / worldConfig.chunkSize);
                    const int cz = (tz >= 0) ? (tz / worldConfig.chunkSize) : ((tz - worldConfig.chunkSize + 1) / worldConfig.chunkSize);

                    Chunk* chunk = m_world->GetChunk(cx, cz);
                    if (!chunk || !chunk->IsGenerated()) {
                        m_world->LoadChunkAt(cx, cz);
                        chunk = m_world->GetChunk(cx, cz);
                    }
                    if (!chunk || !chunk->IsGenerated()) {
                        continue; // non bloccante
                    }

                    int lx = tx % worldConfig.chunkSize; if (lx < 0) lx += worldConfig.chunkSize;
                    int lz = tz % worldConfig.chunkSize; if (lz < 0) lz += worldConfig.chunkSize;
                    const TerrainTile tile = chunk->GetTile(lx, lz); // unica fonte autorevole
                    if (tile.height >= 2.0f && !tile.hasWater) {
                        foundTx = tx;
                        foundTz = tz;
                        foundHeight = tile.height;
                        found = true;
                    }
                }
            }
        }

        if (found) {
            spawnX = (foundTx + 0.5f) * worldConfig.tileSize;
            spawnY = foundHeight * worldConfig.tileSize;
            spawnZ = (foundTz + 0.5f) * worldConfig.tileSize;
            std::cout << "[SPAWN] tile=(" << foundTx << "," << foundTz << ") height=" << foundHeight << std::endl;
        } else {
            std::cerr << "[SPAWN ERROR] No valid land found within radius " << MAX_SPAWN_SEARCH_RADIUS << std::endl;
            spawnX = 0.0f; spawnY = 0.0f; spawnZ = 0.0f;
        }

        m_player = std::make_unique<Player>();
        m_player->SetTileSize(worldConfig.tileSize);
        m_player->SetPosition({spawnX, spawnY, spawnZ});
        m_world->SetPlayerPosition({spawnX, spawnY, spawnZ});

        m_navigation.Initialize(m_world.get(), MAX_STEP_HEIGHT);

        // Connect systems
        m_renderer->SetWorld(m_world.get());
        m_renderer->SetCamera(m_camera.get());

        // Create initial game state
        m_gameState = std::make_unique<MainMenuState>(this);
        m_gameState->Enter();

        m_isRunning = true;
        m_debugMode = true; // Start with debug info visible

        std::cout << "Game initialized successfully! World seed: " << worldConfig.seed << std::endl;

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
        const float deltaTime = m_timer->GetDeltaTime();

        HandleInput();
        Update(deltaTime);
        Render();
    }
}

void Game::SetState(std::unique_ptr<GameState> state) {
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
    if (m_hud) m_hud.reset();
    if (m_audioManager) m_audioManager.reset();
    if (m_assetManager) m_assetManager.reset();
    if (m_camera) m_camera.reset();
    if (m_renderer) m_renderer.reset();
    if (m_timer) m_timer.reset();
    if (m_combatSystem) m_combatSystem.reset();

    m_isRunning = false;
}

void Game::CreateGameSession() {
    // Ricrea sessione completa: World, Player, spawn deterministico, streaming
    // Seed fisso (12345) per nuova partita — non casuale
    WorldConfig worldConfig;
    worldConfig.seed = 12345;
    worldConfig.chunkSize = 16;
    worldConfig.tileSize = 1.0f;
    worldConfig.renderDistance = 3;

    if (!m_world) {
        m_world = std::make_unique<World>();
    }
    if (!m_world->IsInitialized()) {
        if (!m_world->Initialize(worldConfig)) {
            std::cerr << "CreateGameSession: World init failed" << std::endl;
        }
    }

    // Pre-carica chunk attorno origine
    const int preRadius = 1;
    for (int cx = -preRadius; cx <= preRadius; ++cx) {
        for (int cz = -preRadius; cz <= preRadius; ++cz) {
            if (!m_world->GetChunk(cx, cz)) m_world->LoadChunkAt(cx, cz);
        }
    }

    // Spawn deterministico
    float spawnX = 0.5f * worldConfig.tileSize;
    float spawnZ = 0.5f * worldConfig.tileSize;
    float spawnY = 0.0f;
    bool found = false;
    int foundTx = 0, foundTz = 0;
    float foundHeight = 0.0f;
    const int MAX_SPAWN_SEARCH_RADIUS = 64;
    for (int r = 0; r <= MAX_SPAWN_SEARCH_RADIUS && !found; ++r) {
        for (int dx = -r; dx <= r && !found; ++dx) {
            for (int dz = -r; dz <= r && !found; ++dz) {
                if (r > 0 && std::abs(dx) != r && std::abs(dz) != r) continue;
                int tx = dx, tz = dz;
                int cx = (tx >= 0) ? (tx / worldConfig.chunkSize) : ((tx - worldConfig.chunkSize + 1) / worldConfig.chunkSize);
                int cz = (tz >= 0) ? (tz / worldConfig.chunkSize) : ((tz - worldConfig.chunkSize + 1) / worldConfig.chunkSize);
                Chunk* chunk = m_world->GetChunk(cx, cz);
                if (!chunk || !chunk->IsGenerated()) { m_world->LoadChunkAt(cx, cz); chunk = m_world->GetChunk(cx, cz); }
                if (!chunk || !chunk->IsGenerated()) continue;
                int lx = tx % worldConfig.chunkSize; if (lx < 0) lx += worldConfig.chunkSize;
                int lz = tz % worldConfig.chunkSize; if (lz < 0) lz += worldConfig.chunkSize;
                TerrainTile tile = chunk->GetTile(lx, lz);
                if (tile.height >= 2.0f && !tile.hasWater) {
                    foundTx = tx; foundTz = tz; foundHeight = tile.height; found = true; break;
                }
            }
        }
    }
    if (found) {
        spawnX = (foundTx + 0.5f) * worldConfig.tileSize;
        spawnY = foundHeight * worldConfig.tileSize;
        spawnZ = (foundTz + 0.5f) * worldConfig.tileSize;
    } else {
        spawnX = 0.0f; spawnY = 0.0f; spawnZ = 0.0f;
    }

    if (!m_player) m_player = std::make_unique<Player>();
    m_player->SetTileSize(worldConfig.tileSize);
    m_player->SetPosition({spawnX, spawnY, spawnZ});
    m_world->SetPlayerPosition({spawnX, spawnY, spawnZ});
    m_navigation.Initialize(m_world.get(), MAX_STEP_HEIGHT);

    if (m_renderer) {
        m_renderer->SetWorld(m_world.get());
        m_renderer->SetCamera(m_camera.get());
    }
}

void Game::ResetGameSession() {
    if (m_player) m_player.reset();
    if (m_world) m_world.reset();
    m_navigation.Initialize(nullptr, MAX_STEP_HEIGHT);
    m_hoverTileX = -1;
    m_hoverTileZ = -1;
}

void Game::Update(float deltaTime) {
    m_stateChangedThisFrame = false;

    if (m_player) {
        m_player->Update(deltaTime);
        // Single authoritative source for streaming: pass real position to World
        if (m_world) {
            m_world->SetPlayerPosition(m_player->GetPosition());
        }
    }

    if (m_gameState) {
        m_gameState->Update(deltaTime);
    }

    if (m_camera) {
        m_camera->Update(deltaTime);
    }

    if (m_world) {
        m_world->Update(deltaTime); // World handles its own streaming internally
    }

    if (m_audioManager) {
        m_audioManager->UpdateMusic();
    }
}

void Game::Render() {
    if (!m_renderer)
        return;

    m_renderer->BeginFrame();

    if (m_gameState) {
        m_gameState->Render();
    }

    if (m_gameState &&
        m_gameState->GetType() == GameStateType::Playing &&
        m_camera &&
        m_world)
    {
        const Camera3D camera = m_camera->GetCamera();

        m_renderer->BeginMode3D(camera);

        m_world->Render();

        if (m_player) {
            m_player->Render();
        }

        // Hover tile outline stays in 3D.
        if (m_hoverTileX != -9999 && m_hoverTileZ != -9999) {
            const int tx = m_hoverTileX;
            const int tz = m_hoverTileZ;

            const float tileCenterX = static_cast<float>(tx) + 0.5f;
            const float tileCenterZ = static_cast<float>(tz) + 0.5f;
            const float surfaceY = m_world->GetHeightAt(tileCenterX, tileCenterZ) + 0.05f;

            const float s = 1.0f;

            const Vector3 p0 = { static_cast<float>(tx),     surfaceY, static_cast<float>(tz) };
            const Vector3 p1 = { static_cast<float>(tx) + s, surfaceY, static_cast<float>(tz) };
            const Vector3 p2 = { static_cast<float>(tx) + s, surfaceY, static_cast<float>(tz) + s };
            const Vector3 p3 = { static_cast<float>(tx),     surfaceY, static_cast<float>(tz) + s };

            DrawLine3D(p0, p1, YELLOW);
            DrawLine3D(p1, p2, YELLOW);
            DrawLine3D(p2, p3, YELLOW);
            DrawLine3D(p3, p0, YELLOW);
        }

        m_renderer->EndMode3D();

        // FINAL 2D HUD PASS.
        // This is the only place the Player indicator is drawn.
        if (m_hud && m_player) {
            m_hud->Render(*m_player, camera);
        }
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
        if (m_gameState && m_gameState->GetType() == GameStateType::Playing) {
            SetState(std::make_unique<PauseState>(this)); // Playing -> Pause
        } else if (m_gameState && m_gameState->GetType() == GameStateType::MainMenu) {
            m_isRunning = false;
        }
        // Da Pause: resta in Pause; si torna indietro con ENTER sull'opzione 3
    }

    // Delegate to current game state only if no transition occurred this frame
    if (m_gameState && !m_stateChangedThisFrame) {
        m_gameState->HandleInput();
    }

    // Camera input
    if (m_camera) {
        m_camera->HandleInput();
    }

    // Click-to-move + hover tile calculation (only when playing)
    if (m_camera && m_world && m_player && m_gameState && m_gameState->GetType() == GameStateType::Playing) {
        const Ray ray = GetMouseRay(GetMousePosition(), m_camera->GetCamera());

        // Voxel raycast picking
        const float maxDist = 100.0f;
        const float step = 0.5f;
        int bestTileX = -9999;
        int bestTileZ = -9999;

        for (float d = 0.1f; d < maxDist; d += step) {
            const Vector3 point = {
                ray.position.x + ray.direction.x * d,
                ray.position.y + ray.direction.y * d,
                ray.position.z + ray.direction.z * d
            };
            const int tx = (int)std::floor(point.x);
            const int tz = (int)std::floor(point.z);
            const float surfaceY = m_world->GetHeightAt(tx + 0.5f, tz + 0.5f);

            if (point.x >= (float)tx && point.x <= (float)(tx + 1) &&
                point.z >= (float)tz && point.z <= (float)(tz + 1) &&
                point.y >= 0.0f && point.y <= surfaceY) {
                bestTileX = tx;
                bestTileZ = tz;
                break;
            }
        }

        if (bestTileX == -9999 || bestTileZ == -9999) {
            m_hoverTileX = -9999;
            m_hoverTileZ = -9999;
            return;
        }

        m_hoverTileX = bestTileX;
        m_hoverTileZ = bestTileZ;

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            const int curTileX = m_player->GetTileX();
            const int curTileZ = m_player->GetTileZ();

            const std::vector<TileCoord> path = m_navigation.FindPath(curTileX, curTileZ, bestTileX, bestTileZ);

            if (!path.empty()) {
                std::vector<PathNode> pathWithY;
                pathWithY.reserve(path.size());
                for (const auto& t : path) {
                    const float h = m_world->GetHeightAt(t.x + 0.5f, t.z + 0.5f);
                    pathWithY.emplace_back(t.x, t.z, h);
                }
                m_player->SetPath(pathWithY);
            }
        }
    }
}
