#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <raylib.h>
#include "entities/Player.h"
#include "navigation/NavigationSystem.h"
#include "core/CombatSystem.h"
#include "core/QuestLog.h"
#include "core/SaveGame.h"
#include <entities/Mob.h>
#include "ui/HUD.h"
#include "ui/InteractionMenu.h"
#include "rendering/LightingSystem.h"


class GameState;
class Renderer;
class Camera3DController;
class World;
class AssetManager;
class AudioManager;
class Timer;
class HUD;
class ItemVisuals; 
class Village;
struct VillageNpc;

class Game {
public:
    Game();
    ~Game();

    bool Initialize();
    void ResetGameSession();
    void CreateGameSession();
    void StartNewGameLoading(const core::Character& character);
    void AdvanceSessionLoading(float deltaTime);
    float GetLoadingProgress() const;
    const std::string& GetLoadingMessage() const { return m_loadingMessage; }
    bool SaveGame();
    bool LoadGame();
    bool HasSave() const;
    core::QuestLog& GetQuestLog() { return m_questLog; }
    void EnterVillageBuilding(int buildingIndex);
    void ExitVillageBuilding();
    void Run();
    void Shutdown();

    bool IsRunning() const { return m_isRunning; }
    void RequestExit() { m_isRunning = false; }
    void SetState(std::unique_ptr<GameState> state);

    Renderer* GetRenderer() const { return m_renderer.get(); }
    Camera3DController* GetCamera() const { return m_camera.get(); }
    World* GetWorld() const { return m_world.get(); }
    AssetManager* GetAssetManager() const { return m_assetManager.get(); }
    AudioManager* GetAudioManager() const { return m_audioManager.get(); }
    Timer* GetTimer() const { return m_timer.get(); }
    Player* GetPlayer() const { return m_player.get(); }
    HUD* GetHUD() const { return m_hud.get(); }
    core::CombatSystem* GetCombatSystem() const { return m_combatSystem.get(); }
    Mob* GetHoveredMob() const { return m_hoveredMob; }
    void SetHoveredMob(Mob* mob) { m_hoveredMob = mob; }
    core::LightingSystem* GetLightingSystem() const { return m_lightingSystem.get(); }
    ItemVisuals* GetItemVisuals() const { return m_itemVisuals.get(); }
    Village* GetVillage() const { return m_village.get(); }
    int GetActiveBuilding() const { return m_activeBuilding; }
private:
    void Update(float deltaTime);
    void Render();
    void HandleInput();
    void FinishSessionLoading();

    bool m_isRunning;
    bool m_debugMode;
    bool m_stateChangedThisFrame = false;
    bool m_sessionLoading = false;
    bool m_sessionLoadComplete = false;
    bool m_loadingSave = false;
    bool m_hasPendingCharacter = false;

    std::unique_ptr<GameState> m_gameState;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<Camera3DController> m_camera;
    std::unique_ptr<World> m_world;
    std::unique_ptr<AssetManager> m_assetManager;
    std::unique_ptr<AudioManager> m_audioManager;
    std::unique_ptr<Timer> m_timer;
    std::unique_ptr<Player> m_player;
    Model* m_playerModel = nullptr; // loaded once via AssetManager (which owns it)
    ModelAnimation* m_playerAnims = nullptr; // Idle/Walk/Attack, loaded once via AssetManager (which owns it)
    int m_playerAnimCount = 0;
    std::unique_ptr<HUD> m_hud;
    NavigationSystem m_navigation;
    std::unique_ptr<core::CombatSystem> m_combatSystem;
    Mob* m_hoveredMob = nullptr;
    int m_hoverTileX = -1;
    int m_hoverTileZ = -1;
    std::unique_ptr<ItemVisuals> m_itemVisuals;
    std::unique_ptr<core::LightingSystem> m_lightingSystem;
    std::unique_ptr<Village> m_village;
    std::vector<std::pair<int, int>> m_sessionChunks;
    std::size_t m_sessionChunkIndex = 0;
    std::vector<std::string> m_sessionVillageAssets;
    std::size_t m_sessionVillageAssetIndex = 0;
    int m_sessionNpcAssetIndex = 0;
    int m_sessionNpcLoadStage = 0;
    int m_sessionLoadTotalSteps = 1;
    int m_sessionLoadCompletedSteps = 0;
    float m_sessionLoadTimer = 0.0f;
    std::string m_loadingMessage = "Preparazione del mondo";
    core::Character m_pendingCharacter;
    core::SaveGameData m_pendingSaveData;
    const VillageNpc* m_hoveredNpc = nullptr;
    core::QuestLog m_questLog;
    Vector3 m_exteriorPosition{};
    Vector3 m_exteriorCameraTarget{};
    float m_exteriorCameraRotation = 0.0f;
    float m_exteriorCameraZoom = 1.0f;
    int m_activeBuilding = -1;
    int m_pendingBuildingIndex = -1;
    float m_doorTransitionTimer = 0.0f;

    ui::InteractionMenu m_interactionMenu;

    static constexpr float MAX_STEP_HEIGHT = 1.5f;

    static constexpr int SCREEN_WIDTH = 1280;
    static constexpr int SCREEN_HEIGHT = 720;
    static constexpr const char* WINDOW_TITLE = "Isometric RPG";
std::unique_ptr<Mob> m_testMob;
};