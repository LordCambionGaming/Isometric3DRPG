#include "core/Game.h"
#include "navigation/NavigationSystem.h"
#include "states/GameState.h"
#include "states/PauseState.h"
#include "states/MainMenuState.h"
#include "states/DiceRollState.h"
#include "states/PlayingState.h"
#include "states/LoadingState.h"
#include "states/VillageInteractionState.h"
#include "states/VillageInteriorState.h"
#include "entities/Player.h"
#include "core/Timer.h"
#include "core/KeyBindings.h"
#include "core/Localization.h"
#include "core/SaveGame.h"
#include "rendering/Renderer.h"
#include "rendering/Camera3DController.h"
#include "world/World.h"
#include "world/Chunk.h"
#include "world/Village.h"
#include "assets/AssetManager.h"
#include "audio/AudioManager.h"
#include "ui/HUD.h"
#include <raylib.h>
#include "ui/InteractionMenu.h"
#include "rendering/ItemVisuals.h" 
#include "world/TerrainGenerator.h"
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
        lang::Initialize();
        SetTraceLogLevel(LOG_WARNING); // la console non va piu' controllata: silenzia i log informativi di raylib, restano warning/errori

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
            TraceLog(LOG_ERROR, "Failed to initialize renderer!");
            return false;
        }

        core::KeyBindings::Load();

        // Shaders require an active OpenGL context: must be loaded after InitWindow.
        m_lightingSystem->Initialize();
        m_lightingSystem->InitShadows(2048);

        if (!m_assetManager->Initialize()) {
            TraceLog(LOG_ERROR, "Failed to initialize asset manager!");
            return false;
        }

        if (!m_audioManager->Initialize()) {
            TraceLog(LOG_ERROR, "Failed to initialize audio manager!");
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
            TraceLog(LOG_ERROR, "Failed to initialize world!");
            return false;
        }

        // Spawn provvisorio all'origine: quello vero (Village::FindSpawnPosition, costoso: scansiona
        // chunk su chunk) viene calcolato solo quando si avvia davvero una sessione, in modo incrementale,
        // in Game::CreateGameSession() + Game::AdvanceSessionLoading() (loading screen), cosi' l'avvio
        // dell'app non resta bloccato a cercare uno spawn prima ancora di mostrare il menu principale.
        const float spawnX = 0.5f * worldConfig.tileSize;
        const float spawnY = 0.0f;
        const float spawnZ = 0.5f * worldConfig.tileSize;

        m_itemVisuals = std::make_unique<ItemVisuals>(m_assetManager.get(), m_lightingSystem->GetShader());

        m_playerModel = m_assetManager->LoadModel("models/entities/player_animated.glb");
        if (m_playerModel) {
            for (int i = 0; i < m_playerModel->materialCount; i++)
                m_playerModel->materials[i].shader = m_lightingSystem->GetShader();
            m_playerAnims = m_assetManager->LoadAnimations("models/entities/player_animated.glb", m_playerAnimCount);
        }

        m_player = std::make_unique<Player>();
        m_player->SetTileSize(worldConfig.tileSize);
        m_player->SetModel(m_playerModel);
        m_player->SetAnimations(m_playerAnims, m_playerAnimCount);
        m_player->SetItemVisuals(m_itemVisuals.get());
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
        TraceLog(LOG_ERROR, "Exception during initialization: %s", e.what());
        return false;
    }
}

void Game::Run() {
    while (m_isRunning && !m_renderer->ShouldClose()) {
        m_timer->Update();
        // Clamp difensivo: un deltaTime anomalo (hitch, breakpoint, primo frame) non deve far
        // esplodere camera/movimento/streaming dei chunk in un frame solo.
        const float deltaTime = std::min(m_timer->GetDeltaTime(), 0.1f);

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
    m_itemVisuals.reset();
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

// Avvia la generazione di una sessione (mondo + spawn + villaggio) in modo NON bloccante: prepara solo
// l'elenco dei passi da fare (chunk da caricare, modelli del villaggio, NPC) e passa a LoadingState.
// Il lavoro vero e proprio viene spalmato su piu' frame da AdvanceSessionLoading().
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

    // Chunk minimi attorno all'origine, necessari per poter cercare uno spawn valido.
    const int preRadius = 1;
    for (int cx = -preRadius; cx <= preRadius; ++cx) {
        for (int cz = -preRadius; cz <= preRadius; ++cz) {
            if (!m_world->GetChunk(cx, cz)) m_world->LoadChunkAt(cx, cz);
        }
    }

        const Vector3 spawn = Village::FindSpawnPosition(*m_world);

    // 1. Registra/asciuga l'area PRIMA di leggere la quota
    float r = 35.0f + (World::VillageHash(spawn.x, spawn.z) * 15.0f);
    m_world->ForceDryArea(spawn.x, spawn.z, r);

    // 2. Crea player
    if (!m_player) m_player = std::make_unique<Player>();
    m_player->SetTileSize(worldConfig.tileSize);
    m_player->SetModel(m_playerModel);
    m_player->SetAnimations(m_playerAnims, m_playerAnimCount);
    m_player->SetItemVisuals(m_itemVisuals.get());

    // 3. Quota DOPO il flatten: 6.0 = villageHeight usato in World::ForceDryArea.
    //    Se il chunk attorno allo spawn non è ancora caricato, GetHeightAt darebbe
    //    il valore procedurale (sbagliato): usiamo direttamente la costante del flatten.
    Vector3 spawnPos = spawn;
    spawnPos.y = (float)TerrainGenerator::SEA_LEVEL + 1.0f;
    m_player->SetPosition(spawnPos);
    m_world->SetPlayerPosition(spawnPos);

    m_village = std::make_unique<Village>(spawn);
    if (m_camera) m_camera->CenterOn(spawnPos);
    m_navigation.Initialize(m_world.get(), MAX_STEP_HEIGHT);
    
    m_hoveredNpc = nullptr;
    m_testMob.reset();
    m_hoveredMob = nullptr;

    // Elenco dei chunk attorno allo spawn, ordinati per distanza: verranno caricati un po' per frame.
    m_sessionChunks.clear();
    int spawnChunkX = 0, spawnChunkZ = 0;
    m_world->WorldToChunk(spawn.x, spawn.z, spawnChunkX, spawnChunkZ);
    const int radius = m_world->GetGenerationRadius();
    for (int dx = -radius; dx <= radius; ++dx) {
        for (int dz = -radius; dz <= radius; ++dz) {
            if (dx * dx + dz * dz <= radius * radius) {
                m_sessionChunks.push_back({ spawnChunkX + dx, spawnChunkZ + dz });
            }
        }
    }
    std::sort(m_sessionChunks.begin(), m_sessionChunks.end(),
        [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
            const int da = a.first * a.first + a.second * a.second;
            const int db = b.first * b.first + b.second * b.second;
            return da < db;
        });
    m_sessionChunkIndex = 0;

    // Modelli del villaggio e NPC: caricati un po' per frame in AdvanceSessionLoading(), non qui.
    m_sessionVillageAssets = m_village->RequiredModelAssets();
    m_sessionVillageAssetIndex = 0;
    m_sessionNpcAssetIndex = 0;
    m_sessionNpcLoadStage = 0;

    m_sessionLoadTotalSteps = static_cast<int>(m_sessionChunks.size() + m_sessionVillageAssets.size() + 5 + 1);
    m_sessionLoadCompletedSteps = 0;
    m_sessionLoadTimer = 0.0f;
    m_loadingMessage = "Preparazione del mondo";
    m_sessionLoading = true;
    m_sessionLoadComplete = false;

    SetState(std::make_unique<LoadingState>(this));
}

// Chiamato ogni frame da LoadingState mentre m_sessionLoading e' true: fa avanzare il caricamento
// di un passo ogni ~25ms (budget), cosi' il frame resta leggero e la barra di progresso si muove.
void Game::AdvanceSessionLoading(float deltaTime) {
    (void)deltaTime;
    if (!m_sessionLoading) return;

    if (m_sessionChunkIndex < m_sessionChunks.size()) {
        const std::pair<int, int>& coord = m_sessionChunks[m_sessionChunkIndex];
        if (!m_world->GetChunk(coord.first, coord.second)) {
            m_world->LoadChunkAt(coord.first, coord.second);
        }
        m_sessionChunkIndex++;
        m_sessionLoadCompletedSteps++;
        m_loadingMessage = "Preparazione del terreno";
        if (m_audioManager) m_audioManager->UpdateMusic();

        // FIX: appena l'ultimo chunk e' stato caricato, asciuga le aree villaggio sui
        // chunk ORA presenti in m_chunks. In CreateGameSession ForceDryArea girava quando
        // i chunk attorno allo spawn non erano ancora caricati (spawn spesso fuori dai 9
        // chunk di preRadius), quindi l'acqua restava. Qui invece tutti i chunk della
        // sessione sono caricati e ForceDryArea li trova.
        if (m_sessionChunkIndex == m_sessionChunks.size() && m_village && m_world) {
            for (const Vector3& center : m_village->GetVillageCenters()) {
                const float rr = 35.0f + (World::VillageHash(center.x, center.z) * 15.0f);
                m_world->ForceDryArea(center.x, center.z, rr);
            }
        }
        return;
    }

    if (m_sessionVillageAssetIndex < m_sessionVillageAssets.size()) {
        const std::string& asset = m_sessionVillageAssets[m_sessionVillageAssetIndex];
        const std::string modelPath = "models/" + asset;
        Model* model = m_assetManager->LoadModel(modelPath);
        if (model) {
            for (int material = 0; material < model->materialCount; ++material)
                model->materials[material].shader = m_lightingSystem->GetShader();
        }
        m_village->SetModel(asset, model);
        m_sessionVillageAssetIndex++;
        m_sessionLoadCompletedSteps++;
        m_loadingMessage = "Costruzione del villaggio";
        if (m_audioManager) m_audioManager->UpdateMusic();
        return;
    }

    static const std::array<const char*, 5> residentAssets = { {
        "models/villagers/blacksmith.glb", "models/villagers/mayor.glb", "models/villagers/cleric.glb",
        "models/villagers/farmer.glb", "models/villagers/innkeeper.glb"
    } };
    if (m_sessionNpcAssetIndex < static_cast<int>(residentAssets.size())) {
        const std::string asset = residentAssets[static_cast<std::size_t>(m_sessionNpcAssetIndex)];
        Model* model = m_assetManager->LoadModel(asset);
        if (model) {
            for (int material = 0; material < model->materialCount; ++material)
                model->materials[material].shader = m_lightingSystem->GetShader();
        }
        int animationCount = 0;
        ModelAnimation* animations = m_assetManager->LoadAnimations(asset, animationCount);
        m_village->SetNpcAssets(m_sessionNpcAssetIndex, model, animations, animationCount);
        m_sessionNpcAssetIndex++;
        m_sessionLoadCompletedSteps++;
        m_loadingMessage = "Risveglio degli abitanti";
        if (m_audioManager) m_audioManager->UpdateMusic();
        return;
    }

    // Ultimo passo: collega il villaggio alla navigazione, poi si entra in gioco.
    m_navigation.SetStructureBounds(m_village->CollisionBounds());
    m_navigation.SetOccupiedTiles(m_village->NpcPositions());
    m_sessionLoadCompletedSteps++;
    m_sessionLoadComplete = true;
    FinishSessionLoading();
}

float Game::GetLoadingProgress() const {
    if (m_sessionLoadTotalSteps <= 0) return 1.0f;
    return std::min(1.0f, static_cast<float>(m_sessionLoadCompletedSteps) / static_cast<float>(m_sessionLoadTotalSteps));
}

// Applica il personaggio appena creato o il salvataggio in corso di caricamento, poi entra in gioco.
void Game::FinishSessionLoading() {
    m_sessionLoading = false;

    if (m_hasPendingCharacter) {
        if (m_player) m_player->SetCharacter(m_pendingCharacter);
        m_hasPendingCharacter = false;
    }

    if (m_loadingSave) {
        if (m_player) {
            m_player->SetCharacter(m_pendingSaveData.character);
            m_player->SetPosition(m_pendingSaveData.playerPosition);
        }
        m_questLog = m_pendingSaveData.quests;
        if (m_world) m_world->SetPlayerPosition(m_pendingSaveData.playerPosition);
        if (m_pendingSaveData.hasTestMob && m_testMob &&
            m_pendingSaveData.testMobMaxHP == m_testMob->GetMaxHP()) {
            m_testMob->SetPosition(m_pendingSaveData.testMobPosition);
            m_testMob->TakeDamage(m_testMob->GetCurrentHP() - m_pendingSaveData.testMobCurrentHP);
        }
        m_loadingSave = false;
    }

    if (m_renderer) {
        m_renderer->SetWorld(m_world.get());
        m_renderer->SetCamera(m_camera.get());
    }
    if (m_audioManager) {
        // Keep music playing per user request
        m_audioManager->PlaySound("sounds/play.mp3");
    }

    m_interactionMenu.Close();
    m_hoveredMob = nullptr;
    m_hoverTileX = -1;
    m_hoverTileZ = -1;
    m_cameraLocked = true;
if (m_camera && m_player) m_camera->CenterOn(m_player->GetPosition());
    SetState(std::make_unique<PlayingState>(this));
}

void Game::StartNewGameLoading(const core::Character& character) {
    ResetGameSession();
    m_pendingCharacter = character;
    m_hasPendingCharacter = true;
    m_loadingSave = false;
    CreateGameSession();
}

bool Game::HasSave() const {
    return core::SaveGame::Exists();
}

bool Game::SaveGame() {
    if (!m_player) return false;
    core::SaveGameData data;
    data.character = m_player->GetCharacter();
    data.quests = m_questLog;
    data.playerPosition = m_player->GetPosition();
    if (m_testMob) {
        data.hasTestMob = true;
        data.testMobPosition = m_testMob->GetPosition();
        data.testMobCurrentHP = m_testMob->GetCurrentHP();
        data.testMobMaxHP = m_testMob->GetMaxHP();
    }
    return core::SaveGame::Write(data);
}

bool Game::LoadGame() {
    core::SaveGameData data;
    if (!core::SaveGame::Read(data)) return false;

    ResetGameSession();
    m_pendingSaveData = data;
    m_loadingSave = true;
    m_hasPendingCharacter = false;
    CreateGameSession();
    return true;
}

void Game::EnterVillageBuilding(int buildingIndex) {
    if (!m_player || !m_village || buildingIndex < 0 ||
        buildingIndex >= m_village->BuildingCount()) return;
    m_pendingBuildingIndex = -1;
    m_doorTransitionTimer = 0.0f;
    m_exteriorPosition = m_player->GetPosition();
    if (m_camera) {
        m_exteriorCameraTarget = m_camera->GetTarget();
        m_exteriorCameraRotation = m_camera->GetRotation();
        m_exteriorCameraZoom = m_camera->GetZoom();
    }
    m_activeBuilding = buildingIndex;
    m_player->StopMovement({ 0.0f, 0.0f, 2.8f });
    if (m_world) m_world->SetPlayerPosition(m_exteriorPosition);
    if (m_camera) m_camera->SetView(Vector3{ 0.0f, 0.8f, 0.0f }, 3.14159265f * 0.5f, 1.45f);
    m_interactionMenu.Close();
    SetState(std::make_unique<VillageInteriorState>(this, buildingIndex));
}

void Game::ExitVillageBuilding() {
    if (m_gameState && m_gameState->GetType() != GameStateType::VillageInterior) return;
    if (m_player) m_player->StopMovement(m_exteriorPosition);
    if (m_world) m_world->SetPlayerPosition(m_exteriorPosition);
    if (m_camera) {
        m_camera->SetView(m_exteriorCameraTarget, m_exteriorCameraRotation, m_exteriorCameraZoom);
        m_camera->FollowTarget(m_exteriorPosition);
    }
    m_activeBuilding = -1;
    m_pendingBuildingIndex = -1;
    SetState(std::make_unique<PlayingState>(this));
}

void Game::ResetGameSession() {
    if (m_testMob) m_testMob.reset();
    m_village.reset();
    m_hoveredNpc = nullptr;
    m_questLog = {};
    m_activeBuilding = -1;
    m_pendingBuildingIndex = -1;
    m_doorTransitionTimer = 0.0f;
    if (m_player) m_player.reset();
    if (m_world) m_world.reset();
    m_hoveredMob = nullptr;
    m_navigation.Initialize(nullptr, MAX_STEP_HEIGHT);
    m_navigation.SetStructureBounds({});
    m_hoverTileX = -1;
    m_hoverTileZ = -1;
}

void Game::Update(float deltaTime) {
    m_stateChangedThisFrame = false;

    if (m_pendingBuildingIndex >= 0) {
        m_doorTransitionTimer -= deltaTime;
        if (m_doorTransitionTimer <= 0.0f) EnterVillageBuilding(m_pendingBuildingIndex);
    }

    const bool frozen = m_gameState &&
        (m_gameState->GetType() == GameStateType::DiceRoll ||
         m_gameState->GetType() == GameStateType::Paused   || 
         m_gameState->GetType() == GameStateType::Inventory ||
         m_gameState->GetType() == GameStateType::VillageDialogue ||
         m_gameState->GetType() == GameStateType::VillageInterior ||
         // Durante il caricamento il villaggio/i confini di navigazione non sono ancora pronti
         // (SetStructureBounds e' l'ultimo passo di AdvanceSessionLoading): simulare NPC/mondo/camera
         // qui sarebbe lavoro sprecato e con dati incompleti per tutta la durata del loading screen.
         m_gameState->GetType() == GameStateType::Loading) || m_pendingBuildingIndex >= 0;

    if (m_player && !frozen) {
        m_player->Update(deltaTime);
        if (m_world) {
            m_world->SetPlayerPosition(m_player->GetPosition());
        }
        if (m_camera && m_cameraLocked) m_camera->CenterOn(m_player->GetPosition());
    }

    if (m_testMob && !frozen) {
        m_testMob->Update(deltaTime);
    }
    if (m_village && m_world && !frozen) {
    const float timeOfDay = m_lightingSystem ? m_lightingSystem->GetTimeOfDay() : 0.5f;
    std::vector<Vector3> occupiedPositions = m_village->NpcPositions();
    if (m_player) occupiedPositions.push_back(m_player->GetPosition());
    if (m_testMob && m_testMob->IsAlive()) occupiedPositions.push_back(m_testMob->GetPosition());
    m_navigation.SetOccupiedTiles(occupiedPositions);
    const Vector3 villageUpdatePlayerPos = m_player ? m_player->GetPosition() : Vector3{ 0.0f, 0.0f, 0.0f };
    
   bool added = m_village->EnsureVillagesNear(*m_world, villageUpdatePlayerPos);
if (added) {
    for (Vector3 c : m_village->GetVillageCenters()) {
        float r = 35.0f + (World::VillageHash(c.x, c.z) * 15.0f); // 35..50 random per villaggio
        m_world->ForceDryArea(c.x, c.z, r);

    }
    m_navigation.SetStructureBounds(m_village->CollisionBounds());
}

    m_village->Update(*m_world, m_navigation, villageUpdatePlayerPos, deltaTime, timeOfDay);
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

    const GameStateType stateType = m_gameState ? m_gameState->GetType() : GameStateType::Loading;
    const bool interiorDialogue = stateType == GameStateType::VillageDialogue && m_activeBuilding >= 0;
    const bool interiorVisible = stateType == GameStateType::VillageInterior || interiorDialogue;
    if (interiorVisible) m_renderer->SetClearColor(BLACK);
    else if (m_lightingSystem) m_renderer->SetClearColor(m_lightingSystem->GetSkyColor());
    m_renderer->BeginFrame();

    const bool worldVisible = m_gameState &&
        (m_gameState->GetType() == GameStateType::Playing ||
         m_gameState->GetType() == GameStateType::DiceRoll|| 
         m_gameState->GetType() == GameStateType::Inventory ||
         (m_gameState->GetType() == GameStateType::VillageDialogue && !interiorDialogue));
    if (((worldVisible && m_world) || (interiorVisible && m_village)) && m_camera) {
        const Camera3D camera = m_camera->GetCamera();
        if (m_lightingSystem && !interiorVisible) {
    m_lightingSystem->BeginShadowPass(m_player ? m_player->GetPosition() : camera.target);
    m_world->Render(false);                         // solo opachi, l'acqua non fa ombra
    if (m_village) m_village->Render(*m_world, m_player ? m_player->GetPosition() : camera.target);
    if (m_player) m_player->Render();
    m_lightingSystem->EndShadowPass();
}
        m_renderer->BeginMode3D(camera);

        if (m_lightingSystem) m_lightingSystem->BeginLightingMode(camera.position);

        if (interiorVisible) {
            const Vector3 playerPosition = m_player ? m_player->GetPosition() : camera.target;
            const Ray hoverRay = GetMouseRay(GetMousePosition(), camera);
            m_village->RenderInterior(m_activeBuilding, camera.position, playerPosition, hoverRay);
            if (m_player) m_player->Render();
        } else {
            m_world->Render();
            if (m_village) m_village->Render(*m_world,
                m_player ? m_player->GetPosition() : camera.target);
            if (m_hoveredNpc) {
                const Vector3 position = m_hoveredNpc->position;
                DrawBoundingBox({ { position.x - 0.4f, position.y, position.z - 0.4f },
                                  { position.x + 0.4f, position.y + 1.9f, position.z + 0.4f } }, GOLD);
            }
            if (m_player) m_player->Render();
            if (m_testMob) {
                m_testMob->SetHovered(m_hoveredMob == m_testMob.get());
                m_testMob->Render(camera);
            }
        }

        if (m_lightingSystem) m_lightingSystem->EndLightingMode();



        if (worldVisible && m_hoveredMob) DrawBoundingBox(m_hoveredMob->GetBounds(), YELLOW);
        if (worldVisible && m_hoverTileX != -9999 && m_hoverTileZ != -9999) {
            const int tx = m_hoverTileX;
            const int tz = m_hoverTileZ;
            const float surfaceY = m_world->GetHeightAt(tx + 0.5f, tz + 0.5f) + 0.05f;
            const Vector3 p0 = { static_cast<float>(tx), surfaceY, static_cast<float>(tz) };
            const Vector3 p1 = { static_cast<float>(tx) + 1.0f, surfaceY, static_cast<float>(tz) };
            const Vector3 p2 = { static_cast<float>(tx) + 1.0f, surfaceY, static_cast<float>(tz) + 1.0f };
            const Vector3 p3 = { static_cast<float>(tx), surfaceY, static_cast<float>(tz) + 1.0f };
            DrawLine3D(p0, p1, YELLOW);
            DrawLine3D(p1, p2, YELLOW);
            DrawLine3D(p2, p3, YELLOW);
            DrawLine3D(p3, p0, YELLOW);
        }

        m_renderer->EndMode3D();
        if (worldVisible && m_testMob) {
            m_testMob->RenderOverlay(camera);
        }

        if (worldVisible && m_hud && m_player) {
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
        Vector3 playerPos = m_player ? m_player->GetPosition() : Vector3{0,0,0};
        Vector3 currentVillage{};
        Vector3 nextVillage{};
        const Vector3* currentVillagePos = nullptr;
        const Vector3* nextVillagePos = nullptr;

        static float debugVillageTimer = 0.0f;
        static Vector3 cachedNextVillage{};
        
        if (m_world && m_player && m_village) {
            currentVillage = m_village->CenterPosition();
            currentVillagePos = &currentVillage;
            
            // FindNearestVillage is extremely heavy (evaluates multiple region chunks).
            // Do NOT call this every frame. Cache it and update every 1 second.
            debugVillageTimer -= GetFrameTime();
            if (debugVillageTimer <= 0.0f) {
                cachedNextVillage = Village::FindNearestVillage(*m_world, playerPos, currentVillage);
                debugVillageTimer = 1.0f;
            }
            nextVillage = cachedNextVillage;
            nextVillagePos = &nextVillage;
        }
        m_renderer->RenderDebugInfo(m_player ? &playerPos : nullptr,
                                    currentVillagePos, nextVillagePos);
    }

    if (m_audioManager) {
        m_audioManager->UpdateMusic();
    }

    m_renderer->EndFrame();
}

   void Game::HandleInput() {
    if (IsKeyPressed(KEY_F1)) {
        m_debugMode = !m_debugMode;
    }

    if (core::KeyBindings::Pressed(core::KeyBindings::Action::Back)) {
        if (m_pendingBuildingIndex >= 0) {
            m_pendingBuildingIndex = -1;
            m_doorTransitionTimer = 0.0f;
        } else if (m_interactionMenu.IsOpen()) {
            m_interactionMenu.Close();
        } else if (m_gameState && m_gameState->GetType() == GameStateType::Playing) {
            SetState(std::make_unique<PauseState>(this));
        }
    }

   if (m_camera && m_player && m_gameState && m_gameState->GetType() == GameStateType::Playing) {
    if (IsKeyPressed(KEY_Y)) {
        m_cameraLocked = !m_cameraLocked;
        if (m_cameraLocked) m_camera->CenterOn(m_player->GetPosition());
    }
    if (IsKeyDown(KEY_SPACE)) m_camera->CenterOn(m_player->GetPosition());
    }

    if (m_gameState && !m_stateChangedThisFrame) {
        m_gameState->HandleInput();
    }

    const bool modalOpen = m_gameState &&
    (m_gameState->GetType() == GameStateType::DiceRoll ||
        m_gameState->GetType() == GameStateType::Inventory ||
        m_gameState->GetType() == GameStateType::VillageDialogue ||
        m_gameState->GetType() == GameStateType::VillageInterior ||
        m_gameState->GetType() == GameStateType::Loading);
    if (m_camera && !modalOpen){ 
        m_camera->HandleInput();
    }

    if (m_camera && m_world && m_player && m_gameState && m_gameState->GetType() == GameStateType::Playing) {
        const Ray ray = GetMouseRay(GetMousePosition(), m_camera->GetCamera());
        const VillageNpc* hoveredNpc = m_village ? m_village->NpcAtRay(*m_world, ray) : nullptr;
        m_hoveredNpc = hoveredNpc;
        const int rayDoor = m_village ? m_village->BuildingDoorAtRay(*m_world, ray) : -1;
        const bool menuConsumedClick = m_interactionMenu.Update();
        const auto requestBuildingEntry = [this](int buildingIndex) {
            if (!m_village || buildingIndex < 0) return;
            m_interactionMenu.Close();
            const Vector3 door = m_village->BuildingDoorPosition(buildingIndex);
            const Vector3 playerPosition = m_player->GetPosition();
            const float dx = door.x - playerPosition.x;
            const float dz = door.z - playerPosition.z;
            
            // If close enough, show Interaction Menu
            if (dx * dx + dz * dz <= 2.2f * 2.2f) {
                ui::Interactable target;
                target.name = lang::Get("interaction.door"); // we will add this to localization
                target.worldPosition = door;
                target.actions.push_back({ lang::Get("interaction.open").c_str(), [this, buildingIndex]() {
                    // TODO: suono apertura porta
                    m_village->HighlightBuilding(buildingIndex);
                    m_pendingBuildingIndex = buildingIndex;
                    m_doorTransitionTimer = 0.42f;
                }});
                m_interactionMenu.Open(target);
                return;
            }

            // Otherwise walk to it
            const std::vector<TileCoord> path = m_navigation.FindPath(
                m_player->GetTileX(), m_player->GetTileZ(),
                static_cast<int>(std::floor(door.x)), static_cast<int>(std::floor(door.z)));
            if (path.empty()) return;
            std::vector<PathNode> pathWithHeight;
            pathWithHeight.reserve(path.size());
            for (const TileCoord& tile : path) {
                pathWithHeight.emplace_back(tile.x, tile.z,
                    m_world->GetHeightAt(tile.x + 0.5f, tile.z + 0.5f));
            }
            m_player->SetPath(pathWithHeight);
        };
        const auto interactWithNpc = [this](const VillageNpc* npc) {
            if (!npc || !m_player || !m_world) return;
            if (Vector3Distance(m_player->GetPosition(), npc->position) <= 2.2f) {
                ui::Interactable target;
                target.name = npc->id;
                target.worldPosition = npc->position;
                const VillageNpc selectedNpc = *npc;
                target.actions.push_back({ "village.action.talk", [this, selectedNpc]() {
                    // TODO: suono inizio dialogo NPC
                    if (m_player && Vector3Distance(m_player->GetPosition(), selectedNpc.position) <= 2.6f)
                        SetState(std::make_unique<VillageInteractionState>(this, selectedNpc));
                }});
                m_interactionMenu.Open(target);
                return;
            }

            const int npcTileX = static_cast<int>(std::floor(npc->position.x));
            const int npcTileZ = static_cast<int>(std::floor(npc->position.z));
            const int dx[] = { 1, -1, 0, 0 };
            const int dz[] = { 0, 0, 1, -1 };
            for (int direction = 0; direction < 4; ++direction) {
                const int targetX = npcTileX + dx[direction];
                const int targetZ = npcTileZ + dz[direction];
                const std::vector<TileCoord> path = m_navigation.FindPath(
                    m_player->GetTileX(), m_player->GetTileZ(), targetX, targetZ);
                if (path.empty()) continue;
                std::vector<PathNode> pathWithHeight;
                for (const TileCoord& tile : path) {
                    pathWithHeight.emplace_back(tile.x, tile.z,
                        m_world->GetHeightAt(tile.x + 0.5f, tile.z + 0.5f));
                }
                m_player->SetPath(pathWithHeight);
                break;
            }
        };
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
            if (!menuConsumedClick && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (hoveredNpc) interactWithNpc(hoveredNpc);
                else if (rayDoor >= 0) requestBuildingEntry(rayDoor);
            }
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

        

        if (!menuConsumedClick && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (hoveredNpc) {
                interactWithNpc(hoveredNpc);
            } else if (rayDoor >= 0) {
                requestBuildingEntry(rayDoor);
            } else if (m_hoveredMob) {
                // Click su mob: apri menu interazione
                ui::Interactable target;
                target.name          = m_hoveredMob->GetName();
                target.worldPosition = m_hoveredMob->GetPosition();
                Mob* mob = m_hoveredMob;
                target.actions.push_back({ "interaction.attack", [this, mob]() {
                    if (m_player && mob && mob->IsAttackable()) {
                        const core::Character& character = m_player->GetCharacter();
                        const core::WeaponDef weapon = character.ActiveWeapon();
                        if (weapon.grip == core::WeaponGrip::Bow && character.QuiverArrowCount() == 0) {
                            TraceLog(LOG_INFO, "[COMBAT] Non hai frecce per usare l'arco.");
                            return;
                        }
                        const float range = weapon.range;
                        const float dist = Vector3Distance(m_player->GetPosition(), mob->GetPosition());
                        if (dist <= range) {
                            // TODO: suono inizio combattimento / tiro d20
                            SetState(std::make_unique<DiceRollState>(this, mob));
                        } else {
                            TraceLog(LOG_INFO, "[COMBAT] Bersaglio troppo lontano! Distanza: %.2fm (Raggio: %.2fm)", dist, range);
                        }
                    }
                }});
                target.actions.push_back({ "interaction.approach", [this, mob]() {
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
                    target.actions.push_back({ "interaction.interact", []() {} });
                }
                m_interactionMenu.Open(target);
            } else if (m_village && m_village->BuildingDoorAtTile(bestTileX, bestTileZ) >= 0) {
                const int buildingIndex = m_village->BuildingDoorAtTile(bestTileX, bestTileZ);
                requestBuildingEntry(buildingIndex);
            } else {
                // Click su casella vuota: muovi player
                m_interactionMenu.Close();
                const int curTileX = m_player->GetTileX();
                const int curTileZ = m_player->GetTileZ();
                std::vector<Vector3> mobPositions = m_village
                    ? m_village->NpcPositions() : std::vector<Vector3>{};
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