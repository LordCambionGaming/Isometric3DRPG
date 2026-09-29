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

    void RenderWorld();
    void RenderUI();
    void RenderDebugInfo();

    bool ShouldClose() const;

private:
    void DrawGrid(int slices, float spacing);
    void DrawCoordinateSystem();

    World* m_world;
    Camera3DController* m_camera;
    bool m_initialized;
    bool m_showGrid;
    bool m_showCoordinates;
};