#include "rendering/Renderer.h"
#include "rendering/Camera3DController.h"
#include "world/World.h"
#include "core/Timer.h"
#include "core/Localization.h"
#include <raylib.h>
#include <iostream>
#include <fstream>

Renderer::Renderer()
    : m_world(nullptr)
    , m_camera(nullptr)
    , m_initialized(false)
    , m_showGrid(true)
    , m_showCoordinates(true)
    , m_fullscreen(true)
    , m_vsyncEnabled(true)
    , m_windowWidth(1280)
    , m_windowHeight(720)
{
}

Renderer::~Renderer() {
    Shutdown();
}

bool Renderer::Initialize(int screenWidth, int screenHeight, const char* title) {
    try {
        m_windowWidth = screenWidth;
        m_windowHeight = screenHeight;
        std::ifstream settings("game_settings.cfg");
        int fullscreen = 1;
        int vsync = 1;
        int width = screenWidth;
        int height = screenHeight;
        if (settings >> fullscreen >> vsync >> width >> height &&
            width >= 640 && width <= 7680 && height >= 480 && height <= 4320) {
            m_fullscreen = fullscreen != 0;
            m_vsyncEnabled = vsync != 0;
            m_windowWidth = width;
            m_windowHeight = height;
        }

        unsigned int flags = 0;
        if (m_fullscreen) flags |= FLAG_FULLSCREEN_MODE;
        if (m_vsyncEnabled) flags |= FLAG_VSYNC_HINT;
        SetConfigFlags(flags);
        InitWindow(m_windowWidth, m_windowHeight, title);

        if (!IsWindowReady()) {
            std::cerr << "Failed to create window!" << std::endl;
            return false;
        }

        int refreshRate = GetMonitorRefreshRate(GetCurrentMonitor());
        SetTargetFPS(m_vsyncEnabled ? (refreshRate > 0 ? refreshRate : 60) : 60);
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

void Renderer::SetFullscreen(bool enabled) {
    if (enabled == m_fullscreen || !m_initialized) return;
    ToggleFullscreen();
    m_fullscreen = enabled;
    if (!enabled) {
        SetWindowSize(m_windowWidth, m_windowHeight);
        const int monitor = GetCurrentMonitor();
        SetWindowPosition((GetMonitorWidth(monitor) - m_windowWidth) / 2,
                          (GetMonitorHeight(monitor) - m_windowHeight) / 2);
    }
    SaveWindowSettings();
}

void Renderer::SetVSyncEnabled(bool enabled) {
    if (enabled == m_vsyncEnabled || !m_initialized) return;
    m_vsyncEnabled = enabled;
    if (enabled) SetWindowState(FLAG_VSYNC_HINT);
    else ClearWindowState(FLAG_VSYNC_HINT);
    
    int refreshRate = GetMonitorRefreshRate(GetCurrentMonitor());
    SetTargetFPS(enabled ? (refreshRate > 0 ? refreshRate : 60) : 60);
    SaveWindowSettings();
}

void Renderer::SetWindowedResolution(int width, int height) {
    if (width < 640 || width > 7680 || height < 480 || height > 4320) return;
    m_windowWidth = width;
    m_windowHeight = height;
    if (!m_fullscreen && m_initialized) {
        SetWindowSize(width, height);
        const int monitor = GetCurrentMonitor();
        SetWindowPosition((GetMonitorWidth(monitor) - width) / 2,
                          (GetMonitorHeight(monitor) - height) / 2);
    }
    SaveWindowSettings();
}

void Renderer::SaveWindowSettings() const {
    std::ofstream settings("game_settings.cfg", std::ios::trunc);
    if (settings) settings << m_fullscreen << ' ' << m_vsyncEnabled << ' '
                          << m_windowWidth << ' ' << m_windowHeight << '\n';
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

void Renderer::RenderDebugInfo(const Vector3* playerPos, const Vector3* currentVillagePos,
                               const Vector3* nextVillagePos) {
    const int x = 10;
    const int lh = 18;
    int y = 8;

    // Semi-transparent background panel
    DrawRectangle(4, 4, 340, 165, Color{0, 0, 0, 140});
    DrawRectangleLines(4, 4, 340, 165, Color{80, 80, 80, 180});

    DrawText(TextFormat(lang::Get("debug.fps").c_str(), GetFPS()), x, y, 15, Color{100, 255, 100, 255});
    y += lh;

    if (m_camera) {
        Vector3 pos = m_camera->GetPosition();
        DrawText(TextFormat(lang::Get("debug.position").c_str(), pos.x, pos.y, pos.z), x, y, 14, Color{200, 200, 255, 255});
        y += lh;
        DrawText(TextFormat(lang::Get("debug.zoom").c_str(), m_camera->GetZoom()), x, y, 14, Color{200, 200, 255, 255});
        y += lh;
    }

    if (playerPos) {
        DrawText(TextFormat("Player: [%.1f, %.1f, %.1f]", playerPos->x, playerPos->y, playerPos->z), x, y, 14, Color{255, 150, 200, 255});
        y += lh;
    }

    if (currentVillagePos) {
        DrawText(TextFormat("Current village: X %.1f  Z %.1f", currentVillagePos->x,
                            currentVillagePos->z), x, y, 14, Color{255, 215, 160, 255});
        y += lh;
    }

    if (nextVillagePos) {
        DrawText(TextFormat("Next village site: X %.1f  Z %.1f", nextVillagePos->x,
                            nextVillagePos->z), x, y, 14, Color{180, 230, 255, 255});
        y += lh;
    }

    if (m_world) {
        DrawText(TextFormat(lang::Get("debug.seed_chunks").c_str(), m_world->GetSeed(), m_world->GetLoadedChunkCount()), x, y, 13, Color{255, 200, 100, 255});
        y += lh;
    }

    DrawText(lang::Get("debug.controls").c_str(), x, y + 4, 11, Color{150, 150, 150, 200});
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
