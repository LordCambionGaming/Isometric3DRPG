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

        // Deterministic spawn: find valid land tile near origin (0,0)
        // Valid = height >= 2 (SEA_LEVEL) && !hasWater
        float spawnX = 0.5f * worldConfig.tileSize;
        float spawnZ = 0.5f * worldConfig.tileSize;
        float spawnY = 0.0f;
        bool found = false;
        int foundTx = 0, foundTz = 0;
        float foundHeight = 0.0f;
        int foundCx = 0, foundCz = 0;
        int foundLx = 0, foundLz = 0;
        const int MAX_SPAWN_SEARCH_RADIUS = 64;

        std::cout << "[SPAWN SEARCH] START origin=(0,0)" << std::endl;

        // Pre-carica solo chunk attorno all'origine per velocizzare inizio; altre caricate on-demand nel loop
        int preRadius = 1;
        for (int cx = -preRadius; cx <= preRadius; ++cx) {
            for (int cz = -preRadius; cz <= preRadius; ++cz) {
                if (!m_world->GetChunk(cx, cz)) m_world->LoadChunkAt(cx, cz);
            }
        }

        std::cout << "[SPAWN INIT] chunk(0,0)=" << (m_world->GetChunk(0,0) ? "PRESENT" : "MISSING") << " generated=" << (m_world->GetChunk(0,0) && m_world->GetChunk(0,0)->IsGenerated() ? "YES" : "NO") << std::endl;

        // Ricerca deterministica per anelli (raggio crescente), usa solo API esistente
        for (int r = 0; r <= MAX_SPAWN_SEARCH_RADIUS && !found; ++r) {
            std::cout << "[SPAWN SEARCH] radius=" << r << std::endl;
            // Ordine deterministico: per anello, prima per distanza, poi X, poi Z
            // Generiamo tutte le tile dell'anello (per r=0 solo (0,0))
            for (int dx = -r; dx <= r && !found; ++dx) {
                for (int dz = -r; dz <= r && !found; ++dz) {
                    if (r > 0 && std::abs(dx) != r && std::abs(dz) != r) continue; // anello
                    int tx = dx;
                    int tz = dz;

                    int cx = (tx >= 0) ? (tx / worldConfig.chunkSize) : ((tx - worldConfig.chunkSize + 1) / worldConfig.chunkSize);
                    int cz = (tz >= 0) ? (tz / worldConfig.chunkSize) : ((tz - worldConfig.chunkSize + 1) / worldConfig.chunkSize);

                    std::cout << "[SPAWN SEARCH] checking chunk=(" << cx << "," << cz << ")" << std::endl;

                    Chunk* chunk = m_world->GetChunk(cx, cz);
                    if (!chunk || !chunk->IsGenerated()) {
                        m_world->LoadChunkAt(cx, cz);
                        chunk = m_world->GetChunk(cx, cz);
                    }
                    if (!chunk || !chunk->IsGenerated()) {
                        // Se anche dopo LoadChunkAt non generato, salta (non bloccante)
                        continue;
                    }
                    int lx = tx % worldConfig.chunkSize; if (lx < 0) lx += worldConfig.chunkSize;
                    int lz = tz % worldConfig.chunkSize; if (lz < 0) lz += worldConfig.chunkSize;
                    TerrainTile tile = chunk->GetTile(lx, lz); // unica fonte autorevole
                    bool isValid = (tile.height >= 2.0f) && (!tile.hasWater);
                    if (isValid) {
                        int dist = std::abs(tx) + std::abs(tz); // Manhattan distance per ordinamento
                        std::cout << "[SPAWN SELECTED] worldTile=(" << tx << "," << tz << ") chunk=(" << cx << "," << cz << ") local=(" << lx << "," << lz << ") height=" << tile.height << " water=0 distance=" << dist << std::endl;
                        foundTx = tx; foundTz = tz; foundHeight = tile.height;
                        foundCx = cx; foundCz = cz; foundLx = lx; foundLz = lz;
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }
        }

        if (found) {
            spawnX = (foundTx + 0.5f) * worldConfig.tileSize;
            spawnY = foundHeight * worldConfig.tileSize;
            spawnZ = (foundTz + 0.5f) * worldConfig.tileSize;
            std::cout << "[SPAWN SELECTED] worldTile=(" << foundTx << "," << foundTz << ") chunk=(" << foundCx << "," << foundCz << ") local=(" << foundLx << "," << foundLz << ") height=" << foundHeight << " water=0" << std::endl;
        } else {
            std::cerr << "[SPAWN ERROR] No valid land found within radius 64" << std::endl;
            // Non impostare posizione invalida; lasciare Player non inizializzato
            spawnX = 0.0f; spawnY = 0.0f; spawnZ = 0.0f;
        }
        m_player = std::make_unique<Player>();
        m_player->SetTileSize(worldConfig.tileSize);
        m_player->SetPosition({spawnX, spawnY, spawnZ});
        std::cout << "[SPAWN] PLAYER POSITION AFTER SET=(" << spawnX << "," << spawnY << "," << spawnZ << ")" << std::endl;
        if (m_world) {
            m_world->SetPlayerPosition({spawnX, spawnY, spawnZ});
            std::cout << "[WORLD] PLAYER POSITION SET=(" << spawnX << "," << spawnY << "," << spawnZ << ")" << std::endl;
        }

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
        std::cout << "[MOVE UPDATE] position=(" << m_player->GetPosition().x << "," << m_player->GetPosition().y << "," << m_player->GetPosition().z << ")" << std::endl;
        m_player->Update(deltaTime);
        // Single authoritative source for streaming: pass real position to World
        if (m_world) {
            Vector3 p = m_player->GetPosition();
            m_world->SetPlayerPosition(p); // Update streaming center
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
        if (m_player) {
            Vector3 p = m_player->GetPosition(); // UNICA fonte autorevole
            m_player->Render();

            Camera3D cam = m_camera->GetCamera();
            Vector2 screenPos = GetWorldToScreen(p, cam);
            int sw = GetScreenWidth();
            int sh = GetScreenHeight();
            bool onScreen = (screenPos.x >= 0 && screenPos.x <= sw && screenPos.y >= 0 && screenPos.y <= sh);
            // Verifica davanti alla camera senza funzioni non esistenti (manuale)
            Vector3 camPos = cam.position;
            Vector3 diff = {cam.target.x - camPos.x, cam.target.y - camPos.y, cam.target.z - camPos.z};
            float fLen = std::sqrt(diff.x*diff.x + diff.y*diff.y + diff.z*diff.z);
            Vector3 camForward = (fLen > 0.0f) ? Vector3{diff.x/fLen, diff.y/fLen, diff.z/fLen} : Vector3{0.0f, 0.0f, -1.0f};
            Vector3 toDir = {p.x - camPos.x, p.y - camPos.y, p.z - camPos.z};
            float tLen = std::sqrt(toDir.x*toDir.x + toDir.y*toDir.y + toDir.z*toDir.z);
            Vector3 toNorm = (tLen > 0.0f) ? Vector3{toDir.x/tLen, toDir.y/tLen, toDir.z/tLen} : Vector3{0.0f, 0.0f, 0.0f};
            float dotFwd = camForward.x*toNorm.x + camForward.y*toNorm.y + camForward.z*toNorm.z;
            bool visible = onScreen && (dotFwd > 0.0f);

            if (visible) {
                // Scritta 2D sopra il Player proiettato
                DrawText("PLAYER", (int)screenPos.x - 30, (int)screenPos.y - 40, 20, LIME);
            } else {
                // Freccia 2D sul bordo verso il Player (semplice triangolo verso centro schermo -> player)
                int cx = sw / 2, cy = sh / 2;
                float ang = atan2f((float)(cy - screenPos.y), (float)(cx - screenPos.x));
                // Clamped posizione sul bordo
                int mx = cx + (int)(40 * cosf(ang));
                int my = cy + (int)(40 * sinf(ang));
                mx = (mx < 30) ? 30 : (mx > sw - 30 ? sw - 30 : mx);
                my = (my < 30) ? 30 : (my > sh - 30 ? sh - 30 : my);
                // Triangolo semplice orientato verso ang
                DrawTriangle({(float)mx, (float)my}, {(float)(mx - 15*cosf(ang) + 10*sinf(ang)), (float)(my - 15*sinf(ang) - 10*cosf(ang))}, {(float)(mx - 15*cosf(ang) - 10*sinf(ang)), (float)(my - 15*sinf(ang) + 10*cosf(ang))}, RED);
            }
        }

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

        // 3D voxel raycast picking (solo per audit; non modificare architettura funzionante)
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

        std::cout << "[CLICK] mouse=(" << GetMousePosition().x << "," << GetMousePosition().y << ")" << std::endl;
        std::cout << "[CLICK] ray origin=(" << ray.position.x << "," << ray.position.y << "," << ray.position.z << ") direction=(" << ray.direction.x << "," << ray.direction.y << "," << ray.direction.z << ")" << std::endl;
        if (bestTileX != -9999 && bestTileZ != -9999) {
            std::cout << "[CLICK] hit world=(" << bestTileX << "," << bestTileZ << ")" << std::endl;
            int chunkSize = static_cast<int>(m_world->GetConfig().chunkSize);
            int cx = (bestTileX >= 0) ? (bestTileX / chunkSize) : ((bestTileX - chunkSize + 1) / chunkSize);
            int cz = (bestTileZ >= 0) ? (bestTileZ / chunkSize) : ((bestTileZ - chunkSize + 1) / chunkSize);
            int lx = bestTileX % chunkSize; if (lx < 0) lx += chunkSize;
            int lz = bestTileZ % chunkSize; if (lz < 0) lz += chunkSize;
            std::cout << "[CLICK] target tile=(" << bestTileX << "," << bestTileZ << ") chunk=(" << cx << "," << cz << ") local=(" << lx << "," << lz << ")" << std::endl;
            Chunk* ch = m_world->GetChunk(cx, cz);
            if (ch && ch->IsGenerated()) {
                TerrainTile t = ch->GetTile(lx, lz);
                std::cout << "[CLICK TARGET] height=" << t.height << " hasWater=" << (t.hasWater?1:0) << " feature=" << t.waterFeature << std::endl;
                std::cout << "[CLICK CHECK] clickTile=(" << bestTileX << "," << bestTileZ << ") hoverTile=(" << m_hoverTileX << "," << m_hoverTileZ << ") same=" << (bestTileX==m_hoverTileX && bestTileZ==m_hoverTileZ ? "YES" : "NO") << "" << std::endl;
            }
        } else {
            std::cout << "[CLICK] NO HIT (ray did not intersect terrain within maxDist)" << std::endl;
        }
        
        if (bestTileX != -9999 && bestTileZ != -9999) {
            std::cout << "[HOVER] tile=(" << bestTileX << "," << bestTileZ << ")" << std::endl;
            m_hoverTileX = bestTileX;
            m_hoverTileZ = bestTileZ;
            float tileCenterX = bestTileX + 0.5f;
            float tileCenterZ = bestTileZ + 0.5f;
            float tileHeight = m_world->GetHeightAt(tileCenterX, tileCenterZ);
            float surfaceY = tileHeight;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (m_player) {
                    int chunkSize = static_cast<int>(m_world->GetConfig().chunkSize);
                    int curTileX = m_player->GetTileX();
                    int curTileZ = m_player->GetTileZ();

                    // Use NavigationSystem to find path
                    std::vector<TileCoord> path = m_navigation.FindPath(curTileX, curTileZ, bestTileX, bestTileZ);
                    std::cout << "[NAV REQUEST] start=(" << curTileX << "," << curTileZ << ") goal=(" << bestTileX << "," << bestTileZ << ")" << std::endl;
                    // Verifica tile start/goal
                    {
                        int sCX = (curTileX >= 0) ? (curTileX / chunkSize) : ((curTileX - chunkSize + 1) / chunkSize);
                        int sCZ = (curTileZ >= 0) ? (curTileZ / chunkSize) : ((curTileZ - chunkSize + 1) / chunkSize);
                        Chunk* chS = m_world->GetChunk(sCX, sCZ);
                        if (chS && chS->IsGenerated()) {
                            int sLX = curTileX % chunkSize; if (sLX < 0) sLX += chunkSize;
                            int sLZ = curTileZ % chunkSize; if (sLZ < 0) sLZ += chunkSize;
                            TerrainTile sT = chS->GetTile(sLX, sLZ);
                            std::cout << "[NAV TILE] start height=" << sT.height << " water=" << (sT.hasWater?1:0) << " blocked=" << (sT.hasWater ? "YES" : "NO") << std::endl;
                        }
                        int gCX = (bestTileX >= 0) ? (bestTileX / chunkSize) : ((bestTileX - chunkSize + 1) / chunkSize);
                        int gCZ = (bestTileZ >= 0) ? (bestTileZ / chunkSize) : ((bestTileZ - chunkSize + 1) / chunkSize);
                        Chunk* chG = m_world->GetChunk(gCX, gCZ);
                        if (chG && chG->IsGenerated()) {
                            int gLX = bestTileX % chunkSize; if (gLX < 0) gLX += chunkSize;
                            int gLZ = bestTileZ % chunkSize; if (gLZ < 0) gLZ += chunkSize;
                            TerrainTile gT = chG->GetTile(gLX, gLZ);
                            std::cout << "[NAV TILE] goal height=" << gT.height << " water=" << (gT.hasWater?1:0) << " blocked=" << (gT.hasWater ? "YES" : "NO") << std::endl;
                        }
                    }

                    std::cout << "[NAV RESULT] found=" << (!path.empty() ? "true" : "false") << " pathSize=" << path.size() << std::endl;

                    if (!path.empty()) {
                        std::cout << "[CLICK] Path found with " << path.size() << " tiles" << std::endl;
                        std::vector<PathNode> pathWithY;
                        for (const auto& t : path) {
                            float cx = t.x + 0.5f;
                            float cz = t.z + 0.5f;
                            float h = m_world->GetHeightAt(cx, cz);
                            pathWithY.emplace_back(t.x, t.z, h);
                        }
                        std::cout << "[MOVE] path accepted size=" << pathWithY.size() << std::endl;
                        std::cout << "[MOVE] start=(" << curTileX << "," << curTileZ << ") target=(" << bestTileX << "," << bestTileZ << ")" << std::endl;
                        m_player->SetPath(pathWithY);
                        std::cout << "[MOVE] path assigned to Player" << std::endl;
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