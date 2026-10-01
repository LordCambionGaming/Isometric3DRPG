#pragma once

#include <memory>
#include <raylib.h>
#include "player/Player.h"
#include "navigation/NavigationSystem.h"

class GameState;
class Renderer;
class Camera3DController;
class World;
class AssetManager;
class AudioManager;
class Timer;

class Game {
public:
    Game();
    ~Game();

    bool Initialize();
    void Run();
    void Shutdown();

    bool IsRunning() const { return m_isRunning; }
    void RequestExit() { m_isRunning = false; }
    void SetState(std::unique_ptr<GameState> state);

    // Getters for systems
    Renderer* GetRenderer() const { return m_renderer.get(); }
    Camera3DController* GetCamera() const { return m_camera.get(); }
    World* GetWorld() const { return m_world.get(); }
    AssetManager* GetAssetManager() const { return m_assetManager.get(); }
    AudioManager* GetAudioManager() const { return m_audioManager.get(); }
    Timer* GetTimer() const { return m_timer.get(); }

private:
    void Update(float deltaTime);
    void Render();
    void HandleInput();

    bool m_isRunning;
    bool m_debugMode;
    bool m_stateChangedThisFrame = false;

    // Core systems
    std::unique_ptr<GameState> m_gameState;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<Camera3DController> m_camera;
    std::unique_ptr<World> m_world;
    std::unique_ptr<AssetManager> m_assetManager;
    std::unique_ptr<AudioManager> m_audioManager;
    std::unique_ptr<Timer> m_timer;
    std::unique_ptr<Player> m_player;
    NavigationSystem m_navigation;

    int m_hoverTileX = -1;
    int m_hoverTileZ = -1;

    // Movement/walkability
    static constexpr float MAX_STEP_HEIGHT = 1.5f; // Maximum height difference Player can traverse

    // Window configuration
    static constexpr int SCREEN_WIDTH = 1024;
    static constexpr int SCREEN_HEIGHT = 768;
    static constexpr const char* WINDOW_TITLE = "Isometric RPG";
};