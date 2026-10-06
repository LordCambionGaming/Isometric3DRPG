#include "rendering/Renderer.h"
#include "rendering/Camera3DController.h"
#include "world/World.h"
#include "core/Timer.h"
#include <raylib.h>
#include <iostream>

Renderer::Renderer()
    : m_world(nullptr)
    , m_camera(nullptr)
    , m_initialized(false)
    , m_showGrid(true)
    , m_showCoordinates(true)
{
}

Renderer::~Renderer() {
    Shutdown();
}

bool Renderer::Initialize(int screenWidth, int screenHeight, const char* title) {
    try {
        InitWindow(screenWidth, screenHeight, title);

        if (!IsWindowReady()) {
            std::cerr << "Failed to create window!" << std::endl;
            return false;
        }

        SetTargetFPS(60);
        SetExitKey(KEY_NULL); // ESC non chiude finestra � come puzzle3d

        m_initialized = true;
        std::cout << "Renderer initialized: " << screenWidth << "x" << screenHeight << std::endl;

        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in Renderer::Initialize: " << e.what() << std::endl;
        return false;
    }
}

void Renderer::Shutdown() {
    if (m_initialized) {
        CloseWindow();
        m_initialized = false;
    }
}

void Renderer::BeginFrame() {
    BeginDrawing();
    ClearBackground(m_clearColor);
}

void Renderer::EndFrame() {
    EndDrawing();
}

void Renderer::BeginMode3D(Camera3D camera) {
    ::BeginMode3D(camera);
}

void Renderer::EndMode3D() {
    ::EndMode3D();
}

bool Renderer::ShouldClose() const {
    return WindowShouldClose();
}

void Renderer::RenderWorld() {
    if (m_world) {
        m_world->Render();
        // Renderer does NOT select biome; it uses the world/chunk-rendered result
        // which derives BiomeData from BiomeManager (single source of truth)
    }

    // Per-chunk grid is drawn by Chunk::Render (D&D tile grid), not global
    // Global DrawGrid removed per request

    if (false) { // TEMP DISABLED for diagnostic
        DrawCoordinateSystem();
    }
}

void Renderer::RenderUI() {
    // Basic UI rendering - placeholder for future UI systems
}

void Renderer::RenderDebugInfo() {
    const int x = 10;
    const int lh = 18;
    int y = 8;

    // Semi-transparent background panel
    DrawRectangle(4, 4, 260, 100, Color{0, 0, 0, 140});
    DrawRectangleLines(4, 4, 260, 100, Color{80, 80, 80, 180});

    DrawText(TextFormat("FPS: %d", GetFPS()), x, y, 15, Color{100, 255, 100, 255});
    y += lh;

    if (m_camera) {
        Vector3 pos = m_camera->GetPosition();
        DrawText(TextFormat("Pos: %.0f  %.0f  %.0f", pos.x, pos.y, pos.z), x, y, 14, Color{200, 200, 255, 255});
        y += lh;
        DrawText(TextFormat("Zoom: %.2f", m_camera->GetZoom()), x, y, 14, Color{200, 200, 255, 255});
        y += lh;
    }

    if (m_world) {
        DrawText(TextFormat("Seed: %u | Chunks: %d", m_world->GetSeed(), m_world->GetLoadedChunkCount()), x, y, 13, Color{255, 200, 100, 255});
        y += lh;
    }

    DrawText("[F1] Toggle  [ESC] Menu", x, y + 4, 11, Color{150, 150, 150, 200});
}

void Renderer::DrawGrid(int slices, float spacing) {
    int halfSlices = slices / 2;

    for (int i = -halfSlices; i <= halfSlices; i++) {
        Vector3 start = { (float)i * spacing, 0.0f, (float)-halfSlices * spacing };
        Vector3 end = { (float)i * spacing, 0.0f, (float)halfSlices * spacing };
        DrawLine3D(start, end, GRAY);

        start = { (float)-halfSlices * spacing, 0.0f, (float)i * spacing };
        end = { (float)halfSlices * spacing, 0.0f, (float)i * spacing };
        DrawLine3D(start, end, GRAY);
    }
}

void Renderer::DrawCoordinateSystem() {
    const float axisLength = 2.0f;

    // X-axis (red)
    DrawLine3D({ 0.0f, 0.0f, 0.0f }, { axisLength, 0.0f, 0.0f }, RED);

    // Y-axis (green)
    DrawLine3D({ 0.0f, 0.0f, 0.0f }, { 0.0f, axisLength, 0.0f }, GREEN);

    // Z-axis (blue)
    DrawLine3D({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, axisLength }, BLUE);
}
