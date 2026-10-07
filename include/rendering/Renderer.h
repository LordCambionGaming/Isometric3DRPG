#pragma once

#include <raylib.h>
#include <memory>

class World;
class Camera3DController;

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool Initialize(int screenWidth, int screenHeight, const char* title);
    void Shutdown();

    void BeginFrame();
    void EndFrame();

    void BeginMode3D(Camera3D camera);
    void EndMode3D();

    void SetWorld(World* world) { m_world = world; }
    void SetCamera(Camera3DController* camera) { m_camera = camera; }
    void SetClearColor(Color c) { m_clearColor = c; }

    void RenderWorld();
    void RenderUI();
    void RenderDebugInfo();

    bool ShouldClose() const;
    bool IsFullscreen() const { return m_fullscreen; }
    bool IsVSyncEnabled() const { return m_vsyncEnabled; }
    int GetWindowedWidth() const { return m_windowWidth; }
    int GetWindowedHeight() const { return m_windowHeight; }
    void SetFullscreen(bool enabled);
    void SetVSyncEnabled(bool enabled);
    void SetWindowedResolution(int width, int height);

private:
    void DrawGrid(int slices, float spacing);
    void DrawCoordinateSystem();
    void SaveWindowSettings() const;

    World* m_world;
    Camera3DController* m_camera;
    bool m_initialized;
    bool m_showGrid;
    bool m_showCoordinates;
    bool m_fullscreen;
    bool m_vsyncEnabled;
    int m_windowWidth;
    int m_windowHeight;
    Color m_clearColor = SKYBLUE;
};