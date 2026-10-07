#include "core/Game.h"
#include "navigation/NavigationSystem.h"
#include "states/GameState.h"
#include "states/PauseState.h"
#include "states/MainMenuState.h"
#include "states/DiceRollState.h"
#include "entities/Player.h"
#include "core/Timer.h"
#include "rendering/Renderer.h"
#include "rendering/Camera3DController.h"
#include "world/World.h"
#include "world/Chunk.h"
#include "assets/AssetManager.h"
#include "audio/AudioManager.h"
#include "ui/HUD.h"
#include <raylib.h>
#include "ui/InteractionMenu.h"
#include <raymath.h>
#include <iostream>
#include <cmath>
#include <algorithm>

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
    , m_lightingSystem(nullptr)
{
}

Game::~Game() {
    Shutdown();
}

bool Game::Initialize() {
    try {
        m_timer = std::make_unique<Timer>();
        m_assetManager = std::make_unique<AssetManager>();
        m_audioManager = std::make_unique<AudioManager>();
        m_renderer = std::make_unique<Renderer>();
        m_camera = std::make_unique<Camera3DController>();
        m_world = std::make_unique<World>();
        m_hud = std::make_unique<HUD>();
        m_combatSystem = std::make_unique<core::CombatSystem>();
        m_combatSystem->Initialize();
        
        m_lightingSystem = std::make_unique<core::LightingSystem>();
        // NOTE: LightingSystem::Initialize() (LoadShader) is called AFTER InitWindow below.

        if (!m_renderer->Initialize(SCREEN_WIDTH, SCREEN_HEIGHT, WINDOW_TITLE)) {
            std::cerr << "Failed to initialize renderer!" << std::endl;
            return false;
        }
        
        // Shaders require an active OpenGL context: must be loaded after InitWindow.
        m_lightingSystem->Initialize();

        if (!m_assetManager->Initialize()) {
            std::cerr << "Failed to initialize asset manager!" << std::endl;
            return false;
        }

        if (!m_audioManager->Initialize()) {
            std::cerr << "Failed to initialize audio manager!" << std::endl;
            return false;
        }

        m_camera->Initialize();
        m_camera->SetIsometricView();

        WorldConfig worldConfig;
        worldConfig.seed = 12345;
        worldConfig.chunkSize = 16;
        worldConfig.tileSize = 1.0f;
        worldConfig.renderDistance = 3;

        m_world->SetGlobalShader(m_lightingSystem->GetShader());
        if (!m_world->Initialize(worldConfig)) {
            std::cerr << "Failed to initialize world!" << std::endl;
            return false;
        }

        float spawnX = 0.5f * worldConfig.tileSize;
        float spawnZ = 0.5f * worldConfig.tileSize;
        float spawnY = 0.0f;
        bool found = false;
        int foundTx = 0, foundTz = 0;
        float foundHeight = 0.0f;
        const int MAX_SPAWN_SEARCH_RADIUS = 64;
        
        const int preRadius = 1;
        for (int cx = -preRadius; cx <= preRadius; ++cx) {
            for (int cz = -preRadius; cz <= preRadius; ++cz) {
                if (!m_world->GetChunk(cx, cz)) m_world->LoadChunkAt(cx, cz);
            }
        }

        for (int r = 0; r <= MAX_SPAWN_SEARCH_RADIUS && !found; ++r) {
            for (int dx = -r; dx <= r && !found; ++dx) {
                for (int dz = -r; dz <= r && !found; ++dz) {
                    if (r > 0 && std::abs(dx) != r && std::abs(dz) != r) continue;
                    const int tx = dx;
                    const int tz = dz;

                    const int cx = (tx >= 0) ? (tx / worldConfig.chunkSize) : ((tx - worldConfig.chunkSize + 1) / worldConfig.chunkSize);
                    const int cz = (tz >= 0) ? (tz / worldConfig.chunkSize) : ((tz - worldConfig.chunkSize + 1) / worldConfig.chunkSize);

                    Chunk* chunk = m_world->GetChunk(cx, cz);
                    if (!chunk || !chunk->IsGenerated()) {
                        m_world->LoadChunkAt(cx, cz);
                        chunk = m_world->GetChunk(cx, cz);
                    }
                    if (!chunk || !chunk->IsGenerated()) continue;

                    int lx = tx % worldConfig.chunkSize; if (lx < 0) lx += worldConfig.chunkSize;
                    int lz = tz % worldConfig.chunkSize; if (lz < 0) lz += worldConfig.chunkSize;
                    const TerrainTile tile = chunk->GetTile(lx, lz);
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
        } else {
            spawnX = 0.0f; spawnY = 0.0f; spawnZ = 0.0f;
        }

        m_playerModel = m_assetManager->LoadModel("models/entities/player_animated.glb");
        for (int i = 0; i < m_playerModel->materialCount; i++) { m_playerModel->materials[i].shader = m_lightingSystem->GetShader(); }
        if (m_playerModel) {
            m_playerAnims = m_assetManager->LoadAnimations("models/entities/player_animated.glb", m_playerAnimCount);
        }

        m_player = std::make_unique<Player>();
        m_player->SetTileSize(worldConfig.tileSize);
        m_player->SetModel(m_playerModel);
        m_player->SetAnimations(m_playerAnims, m_playerAnimCount);
        m_player->SetPosition({spawnX, spawnY, spawnZ});
        m_world->SetPlayerPosition({spawnX, spawnY, spawnZ});

        m_navigation.Initialize(m_world.get(), MAX_STEP_HEIGHT);
        
        m_renderer->SetWorld(m_world.get());
        m_renderer->SetCamera(m_camera.get());

        m_gameState = std::make_unique<MainMenuState>(this);
        m_gameState->Enter();

        m_isRunning = true;
        m_debugMode = true;

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

    if (m_testMob) m_testMob.reset();
    if (m_world) m_world.reset();
    if (m_hud) m_hud.reset();
    if (m_audioManager) m_audioManager.reset();
    if (m_assetManager) m_assetManager.reset();
    if (m_camera) m_camera.reset();
    if (m_renderer) m_renderer.reset();
    if (m_timer) m_timer.reset();
    if (m_combatSystem) m_combatSystem.reset();
    if (m_lightingSystem) {
        m_lightingSystem->Unload();
        m_lightingSystem.reset();
    }

    m_isRunning = false;
}

void Game::CreateGameSession() {
    WorldConfig worldConfig;
    worldConfig.seed = 12345;
    worldConfig.chunkSize = 16;
    worldConfig.tileSize = 1.0f;
    worldConfig.renderDistance = 3;

    if (!m_world) m_world = std::make_unique<World>();
    if (!m_world->IsInitialized()) {
        m_world->SetGlobalShader(m_lightingSystem->GetShader());
        m_world->Initialize(worldConfig);
    }

    const int preRadius = 1;
    for (int cx = -preRadius; cx <= preRadius; ++cx) {
        for (int cz = -preRadius; cz <= preRadius; ++cz) {
            if (!m_world->GetChunk(cx, cz)) m_world->LoadChunkAt(cx, cz);
        }
    }

    float spawnX = 0.5f, spawnY = 0.0f, spawnZ = 0.5f;
    bool found = false;
    int foundTx = 0, foundTz = 0;
    float foundHeight = 0.0f;
    for (int r = 0; r <= 64 && !found; ++r) {
        for (int dx = -r; dx <= r && !found; ++dx) {
            for (int dz = -r; dz <= r && !found; ++dz) {
                if (r > 0 && std::abs(dx) != r && std::abs(dz) != r) continue;
                int tx = dx, tz = dz;
                int cx = (tx >= 0) ? (tx / 16) : ((tx - 15) / 16);
                int cz = (tz >= 0) ? (tz / 16) : ((tz - 15) / 16);
                Chunk* chunk = m_world->GetChunk(cx, cz);
                if (!chunk || !chunk->IsGenerated()) { m_world->LoadChunkAt(cx, cz); chunk = m_world->GetChunk(cx, cz); }
                if (!chunk || !chunk->IsGenerated()) continue;
                int lx = tx % 16; if (lx < 0) lx += 16;
                int lz = tz % 16; if (lz < 0) lz += 16;
                TerrainTile tile = chunk->GetTile(lx, lz);
                if (tile.height >= 2.0f && !tile.hasWater) {
                    foundTx = tx; foundTz = tz; foundHeight = tile.height; found = true; break;
                }
            }
        }
    }
    if (found) {
        spawnX = (foundTx + 0.5f);
        spawnY = foundHeight;
        spawnZ = (foundTz + 0.5f);
    }

    if (!m_player) m_player = std::make_unique<Player>();
    m_player->SetTileSize(worldConfig.tileSize);
    m_player->SetModel(m_playerModel);
    m_player->SetAnimations(m_playerAnims, m_playerAnimCount);
    m_player->SetPosition({spawnX, spawnY, spawnZ});
    m_world->SetPlayerPosition({spawnX, spawnY, spawnZ});
    m_navigation.Initialize(m_world.get(), MAX_STEP_HEIGHT);

    // Creiamo un mostro di prova reale (Orco con 20 HP e CA 12)
   Vector3 mobPos = { spawnX + 2.0f, spawnY, spawnZ + 2.0f };
m_testMob = std::make_unique<Enemy>("Orco di Prova", mobPos, 20, 12);
m_hoveredMob = m_testMob.get();

    if (m_renderer) {
        m_renderer->SetWorld(m_world.get());
        m_renderer->SetCamera(m_camera.get());
    }
}


void Game::ResetGameSession() {
    if (m_testMob) m_testMob.reset();
    if (m_player) m_player.reset();
    if (m_world) m_world.reset();
    m_hoveredMob = nullptr;
    m_navigation.Initialize(nullptr, MAX_STEP_HEIGHT);
    m_hoverTileX = -1;
    m_hoverTileZ = -1;
}

void Game::Update(float deltaTime) {
    m_stateChangedThisFrame = false;

    const bool frozen = m_gameState &&
        (m_gameState->GetType() == GameStateType::DiceRoll ||
         m_gameState->GetType() == GameStateType::Paused   || 
         m_gameState->GetType() == GameStateType::Inventory);

    if (m_player && !frozen) {
        m_player->Update(deltaTime);
        if (m_world) {
            m_world->SetPlayerPosition(m_player->GetPosition());
        }
    }

    if (m_testMob && !frozen) {
        m_testMob->Update(deltaTime);
    }

    if (m_gameState) {
        m_gameState->Update(deltaTime);
    }

    if (m_camera && !frozen) {
        m_camera->Update(deltaTime);
    }

    if (m_world && !frozen) {
        m_world->Update(deltaTime);
    }

    if (m_audioManager) {
        m_audioManager->UpdateMusic();
    }
    
    if (m_lightingSystem && !frozen) {
        m_lightingSystem->UpdateTimeOfDay(deltaTime);
    }
}

void Game::Render() {
    if (!m_renderer) return;

    if(m_lightingSystem) m_renderer->SetClearColor(m_lightingSystem->GetSkyColor());
    m_renderer->BeginFrame();

    const bool worldVisible = m_gameState &&
        (m_gameState->GetType() == GameStateType::Playing ||
         m_gameState->GetType() == GameStateType::DiceRoll|| 
         m_gameState->GetType() == GameStateType::Inventory);

    if (worldVisible && m_camera && m_world) {
        const Camera3D camera = m_camera->GetCamera();

        m_renderer->BeginMode3D(camera);

        if (m_lightingSystem) m_lightingSystem->BeginLightingMode();
        
        m_world->Render();

        if (m_player) {
            m_player->Render();
        }

        if (m_testMob) {
            m_testMob->SetHovered(m_hoveredMob == m_testMob.get());
            m_testMob->Render(camera);
        }
        if (m_hoveredMob) DrawBoundingBox(m_hoveredMob->GetBounds(), YELLOW);

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

        if (m_lightingSystem) m_lightingSystem->EndLightingMode();

        m_renderer->EndMode3D();
         if (m_testMob) {
            m_testMob->RenderOverlay(camera);
        }

        if (m_hud && m_player) {
            m_hud->Render(*m_player, camera, m_lightingSystem.get());
        }
    }

        // Draw interaction menu in 2D screen-space (outside Mode3D & shader)
        if (m_interactionMenu.IsOpen() && m_camera) {
            m_interactionMenu.Draw(m_camera->GetCamera());
        }

    if (m_gameState) {
        m_gameState->Render();
    }

    if (m_debugMode) {
        m_renderer->RenderDebugInfo();
    }

    m_renderer->EndFrame();
}

   void Game::HandleInput() {
    if (IsKeyPressed(KEY_F1)) {
        m_debugMode = !m_debugMode;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        if (m_interactionMenu.IsOpen()) {
            m_interactionMenu.Close();
        } else if (m_gameState && m_gameState->GetType() == GameStateType::Playing) {
            SetState(std::make_unique<PauseState>(this));
        } else if (m_gameState && m_gameState->GetType() == GameStateType::MainMenu) {
            m_isRunning = false;
        }
    }

    if (m_camera && m_gameState && m_gameState->GetType() == GameStateType::Playing) {

    }

    if (m_gameState && !m_stateChangedThisFrame) {
        m_gameState->HandleInput();
    }

    const bool modalOpen = m_gameState &&
    (m_gameState->GetType() == GameStateType::DiceRoll ||
     m_gameState->GetType() == GameStateType::Inventory);
    if (m_camera && !modalOpen){ 
        m_camera->HandleInput();
    }

    if (m_camera && m_world && m_player && m_gameState && m_gameState->GetType() == GameStateType::Playing) {
        const Ray ray = GetMouseRay(GetMousePosition(), m_camera->GetCamera());
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
            m_hoveredMob = nullptr;
            return;
        }

        m_hoverTileX = bestTileX;
        m_hoverTileZ = bestTileZ;
        m_hoveredMob = nullptr;
        if (m_testMob && m_testMob->IsAlive()) {
            const Vector3 mp = m_testMob->GetPosition();
            if ((int)std::floor(mp.x) == bestTileX && (int)std::floor(mp.z) == bestTileZ)
            m_hoveredMob = m_testMob.get();
        }

        

        bool menuConsumedClick = m_interactionMenu.Update();

        if (!menuConsumedClick && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (m_hoveredMob) {
                // Click su mob: apri menu interazione
                ui::Interactable target;
                target.name          = m_hoveredMob->GetName();
                target.worldPosition = m_hoveredMob->GetPosition();
                Mob* mob = m_hoveredMob;
                target.actions.push_back({ "Attacca", [this, mob]() {
                    if (m_player && mob && mob->IsAttackable()) {
                        const float range = m_player->GetCharacter().ActiveWeapon().range;
                        const float dist = Vector3Distance(m_player->GetPosition(), mob->GetPosition());
                        if (dist <= range) {
                            SetState(std::make_unique<DiceRollState>(this, mob));
                        } else {
                            TraceLog(LOG_INFO, "[COMBAT] Bersaglio troppo lontano! Distanza: %.2fm (Raggio: %.2fm)", dist, range);
                        }
                    }
                }});
                target.actions.push_back({ "Avvicinati", [this, mob]() {
                    const Vector3 mp = mob->GetPosition();
                    const int tx = (int)std::floor(mp.x);
                    const int tz = (int)std::floor(mp.z);
                    const int dx[] = {1,-1,0,0};
                    const int dz[] = {0,0,1,-1};
                    for (int d = 0; d < 4; ++d) {
                        int ntx = tx + dx[d], ntz = tz + dz[d];
                        auto path = m_navigation.FindPath(m_player->GetTileX(), m_player->GetTileZ(), ntx, ntz);
                        if (!path.empty()) {
                            std::vector<PathNode> pathWithY;
                            for (const auto& t : path) {
                                float h = m_world->GetHeightAt(t.x + 0.5f, t.z + 0.5f);
                                pathWithY.emplace_back(t.x, t.z, h);
                            }
                            m_player->SetPath(pathWithY);
                            break;
                        }
                    }
                }});
                if (mob->GetType() != MobType::Enemy) {
                    target.actions.push_back({ "Interagisci", []() {} });
                }
                m_interactionMenu.Open(target);
            } else {
                // Click su casella vuota: muovi player
                m_interactionMenu.Close();
                const int curTileX = m_player->GetTileX();
                const int curTileZ = m_player->GetTileZ();
                std::vector<Vector3> mobPositions;
                if (m_testMob && m_testMob->IsAlive())
                    mobPositions.push_back(m_testMob->GetPosition());
                m_navigation.SetOccupiedTiles(mobPositions);
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

    if (m_gameState && m_gameState->GetType() == GameStateType::Playing) {
        if (IsKeyPressed(KEY_N) && m_testMob) {
            m_testMob->TakeDamage(3);
        }
        if (IsKeyPressed(KEY_M) && m_testMob) {
            m_testMob->Heal(20);
        }
    }

}




