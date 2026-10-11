#include "rendering/LightingSystem.h"
#include <cmath>
#include <algorithm>
#include <rlgl.h>

#include <raymath.h>
namespace {

struct Atmos { Vector3 sky; Vector3 amb; float ambI; };
struct Key   { float t; Atmos a; };

float SmoothStep(float a, float b, float x) {
    float t = (x - a) / (b - a);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return t * t * (3.0f - 2.0f * t);
}
Vector3 LerpV(Vector3 a, Vector3 b, float f) {
    return { a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f, a.z + (b.z - a.z) * f };
}

// Cielo e ambiente nel tempo (t: 0 = mezzanotte, 0.25 = alba, 0.5 = mezzogiorno, 0.75 = tramonto).
// La notte è blu scuro ma non nera: ambiente 0.45 con tinta fredda.
Atmos EvalAtmos(float t) {
    const Atmos night{ {0.07f, 0.10f, 0.24f}, {0.30f, 0.38f, 0.65f}, 0.45f };
    const Atmos dawn { {0.80f, 0.47f, 0.32f}, {0.85f, 0.65f, 0.60f}, 0.45f };
    const Atmos day  { {0.24f, 0.47f, 0.78f}, {0.60f, 0.60f, 0.60f}, 0.40f };
    const Atmos dusk { {0.86f, 0.40f, 0.24f}, {0.80f, 0.55f, 0.50f}, 0.45f };
    const Key keys[] = {
        {0.00f, night}, {0.20f, night}, {0.27f, dawn}, {0.36f, day},
        {0.64f, day},   {0.73f, dusk},  {0.82f, night}, {1.00f, night}
    };
    const int n = (int)(sizeof(keys) / sizeof(keys[0]));
    for (int i = 0; i < n - 1; ++i) {
        if (t <= keys[i + 1].t) {
            const float f = SmoothStep(keys[i].t, keys[i + 1].t, t);
            return { LerpV(keys[i].a.sky, keys[i + 1].a.sky, f),
                     LerpV(keys[i].a.amb, keys[i + 1].a.amb, f),
                     keys[i].a.ambI + (keys[i + 1].a.ambI - keys[i].a.ambI) * f };
        }
    }
    return keys[n - 1].a;
}

} // namespace

namespace core {

LightingSystem::LightingSystem() : m_timeOfDay(0.3f), m_shaderLoaded(false) {}

LightingSystem::~LightingSystem() {
    Unload();
}

void LightingSystem::Initialize() {
    Shader sh = LoadShader("assets/shaders/lighting.vs", "assets/shaders/lighting.fs");
    if (sh.id == 0 || sh.locs == nullptr) {
        TraceLog(LOG_ERROR, "[LIGHT] lighting.vs/.fs non si compilano: uso lo shader di default di raylib");
        m_lightingShader = Shader{ rlGetShaderIdDefault(), rlGetShaderLocsDefault() };
        m_shaderLoaded = false;
        return;
    }
    m_lightingShader = sh;
    m_shaderLoaded = true;

    m_lightingShader.locs[SHADER_LOC_MATRIX_MVP]    = GetShaderLocation(m_lightingShader, "mvp");
    m_lightingShader.locs[SHADER_LOC_MATRIX_MODEL]  = GetShaderLocation(m_lightingShader, "matModel");
    m_lightingShader.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(m_lightingShader, "matNormal");
    m_locViewPos = GetShaderLocation(m_lightingShader, "viewPos");
    
    
        
        m_locAmbientColor = GetShaderLocation(m_lightingShader, "ambientColor");
        m_locAmbientIntensity = GetShaderLocation(m_lightingShader, "ambientIntensity");
        m_locLightsCount = GetShaderLocation(m_lightingShader, "lightsCount");
        m_locLightPositions = GetShaderLocation(m_lightingShader, "lightPositions");
        m_locLightColors = GetShaderLocation(m_lightingShader, "lightColors");
        m_locLightRadii = GetShaderLocation(m_lightingShader, "lightRadii");
        m_locSunDirection = GetShaderLocation(m_lightingShader, "sunDirection");
        m_locSunColor = GetShaderLocation(m_lightingShader, "sunColor");
        int units[4] = { 0, 1, 2, 3 };
        const char* names[4] = { "texture0", "texture1", "texture2", "texture3" };
        for (int i = 0; i < 4; ++i) {
         int loc = GetShaderLocation(m_lightingShader, names[i]);
            if (loc >= 0) SetShaderValue(m_lightingShader, loc, &units[i], SHADER_UNIFORM_INT);
        }
        m_locFogColor   = GetShaderLocation(m_lightingShader, "fogColor");
        m_locFogDensity = GetShaderLocation(m_lightingShader, "fogDensity");
        m_locExposure   = GetShaderLocation(m_lightingShader, "exposure");
    
}

void LightingSystem::Unload() {
    if (m_shaderLoaded) {
        UnloadShader(m_lightingShader);
        m_shaderLoaded = false;
    }
}

void LightingSystem::UpdateTimeOfDay(float deltaTime) {
    // 1 day = 10 minutes real time (600 seconds) -> 0.00166 per second
    m_timeOfDay += deltaTime / 600.0f;
    if (m_timeOfDay >= 1.0f) m_timeOfDay -= 1.0f;
}

Color LightingSystem::GetSkyColor() const {
    const Atmos a = EvalAtmos(m_timeOfDay);
    return Color{ (unsigned char)(a.sky.x * 255.0f), (unsigned char)(a.sky.y * 255.0f),
                  (unsigned char)(a.sky.z * 255.0f), 255 };
}

void LightingSystem::ClearLights() {
    m_lights.clear();
}

void LightingSystem::AddPointLight(Vector3 pos, Color color, float radius) {
    m_lights.push_back({pos, color, radius});   // niente limite qui
}


void LightingSystem::BeginLightingMode(Vector3 cameraPos) {
    if (!m_shaderLoaded) return;

    const Atmos a = EvalAtmos(m_timeOfDay);
    const float sinA = sinf((m_timeOfDay - 0.25f) * 2.0f * PI);
    const float cosA = cosf((m_timeOfDay - 0.25f) * 2.0f * PI);

    // Sole di giorno, luna di notte. Il cambio avviene quando entrambi sono a intensità 0: nessun salto visibile.
    Vector3 sunDir, sunColor;
    if (sinA >= 0.0f) {
        sunDir = { cosA, sinA, 0.5f };
        const float k    = SmoothStep(0.0f, 0.25f, sinA);          // sale piano dopo l'alba
        const float warm = 1.0f - SmoothStep(0.0f, 0.5f, sinA);    // arancio vicino all'orizzonte
        sunColor = { 1.0f * k, (0.92f - 0.32f * warm) * k, (0.82f - 0.52f * warm) * k };
    } else {
        sunDir = { -cosA, -sinA, 0.5f };
        const float k = SmoothStep(0.0f, 0.3f, -sinA) * 0.7f;
        sunColor = { 0.45f * k, 0.55f * k, 0.90f * k };            // luce lunare fredda
    }

    SetShaderValue(m_lightingShader, m_locAmbientColor, &a.amb, SHADER_UNIFORM_VEC3);
    SetShaderValue(m_lightingShader, m_locAmbientIntensity, &a.ambI, SHADER_UNIFORM_FLOAT);
    SetShaderValue(m_lightingShader, m_locSunDirection, &sunDir, SHADER_UNIFORM_VEC3);
    SetShaderValue(m_lightingShader, m_locSunColor, &sunColor, SHADER_UNIFORM_VEC3);

    // Max 8 luci nello shader: tieni le più vicine
    std::sort(m_lights.begin(), m_lights.end(), [&](const PointLight& l, const PointLight& r) {
        auto d2 = [&](const PointLight& p) {
            const float dx = p.position.x - cameraPos.x, dy = p.position.y - cameraPos.y, dz = p.position.z - cameraPos.z;
            return dx * dx + dy * dy + dz * dz;
        };
        return d2(l) < d2(r);
    });
    if (m_lights.size() > 8) m_lights.resize(8);

    int lightsCount = static_cast<int>(m_lights.size());
    SetShaderValue(m_lightingShader, m_locLightsCount, &lightsCount, SHADER_UNIFORM_INT);
    if (lightsCount > 0) {
        float positions[8 * 3], colors[8 * 3], radii[8];
        for (int i = 0; i < lightsCount; ++i) {
            positions[i*3+0] = m_lights[i].position.x; positions[i*3+1] = m_lights[i].position.y; positions[i*3+2] = m_lights[i].position.z;
            colors[i*3+0] = m_lights[i].color.r / 255.0f; colors[i*3+1] = m_lights[i].color.g / 255.0f; colors[i*3+2] = m_lights[i].color.b / 255.0f;
            radii[i] = m_lights[i].radius;
        }
        SetShaderValueV(m_lightingShader, m_locLightPositions, positions, SHADER_UNIFORM_VEC3, lightsCount);
        SetShaderValueV(m_lightingShader, m_locLightColors, colors, SHADER_UNIFORM_VEC3, lightsCount);
        SetShaderValueV(m_lightingShader, m_locLightRadii, radii, SHADER_UNIFORM_FLOAT, lightsCount);
    }

    float fogDensity = 0.010f;
    float exposure   = 1.05f;
    SetShaderValue(m_lightingShader, m_locFogColor,   &a.sky,      SHADER_UNIFORM_VEC3);
    SetShaderValue(m_lightingShader, m_locFogDensity, &fogDensity, SHADER_UNIFORM_FLOAT);
    SetShaderValue(m_lightingShader, m_locExposure,   &exposure,   SHADER_UNIFORM_FLOAT);
    if (m_locViewPos >= 0) SetShaderValue(m_lightingShader, m_locViewPos, &cameraPos, SHADER_UNIFORM_VEC3);

    int on = m_shadowsThisFrame ? 1 : 0;
SetShaderValue(m_lightingShader, m_locShadowsOn, &on, SHADER_UNIFORM_INT);
if (on) SetShaderValueMatrix(m_lightingShader, m_locLightVP, m_lightVP);
if (on) { rlActiveTextureSlot(10); rlEnableTexture(m_shadowRT.depth.id); rlActiveTextureSlot(0); }
m_shadowsThisFrame = false;
    BeginShaderMode(m_lightingShader);
}

void LightingSystem::AddTorch(Vector3 pos, float radius, float strength) {
    const float t = (float)GetTime();
    const float seed = pos.x * 12.9898f + pos.z * 78.233f;   // ogni torcia sfarfalla in modo diverso
    const float flicker = 1.0f + 0.07f * sinf(t * 9.0f + seed) + 0.04f * sinf(t * 23.0f + seed * 1.7f);
    Color c = { (unsigned char)(255 * strength), (unsigned char)(165 * strength), (unsigned char)(70 * strength), 255 };
    AddPointLight(pos, c, radius * flicker);
}

void LightingSystem::EndLightingMode() {
    if (!m_shaderLoaded) return;
    EndShaderMode();
}

void LightingSystem::InitShadows(int size) {
    if (!m_shaderLoaded) return;
    m_shadowSize = size;
    m_shadowRT.id = rlLoadFramebuffer();   // raylib 5.5: rlLoadFramebuffer() senza argomenti
    if (m_shadowRT.id == 0) return;
    rlEnableFramebuffer(m_shadowRT.id);
    m_shadowRT.depth.id = rlLoadTextureDepth(size, size, false);
    m_shadowRT.depth.width = size; m_shadowRT.depth.height = size;
    rlFramebufferAttach(m_shadowRT.id, m_shadowRT.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
    m_shadowsReady = rlFramebufferComplete(m_shadowRT.id);
    rlDisableFramebuffer();
    m_shadowRT.texture.width = size; m_shadowRT.texture.height = size;

    m_locLightVP     = GetShaderLocation(m_lightingShader, "lightVP");
    m_locShadowMap   = GetShaderLocation(m_lightingShader, "shadowMap");
    m_locShadowsOn   = GetShaderLocation(m_lightingShader, "shadowsOn");
    m_locShadowTexel = GetShaderLocation(m_lightingShader, "shadowTexel");
    int slot = 10;
    SetShaderValue(m_lightingShader, m_locShadowMap, &slot, SHADER_UNIFORM_INT);
    float texel = 1.0f / size;
    SetShaderValue(m_lightingShader, m_locShadowTexel, &texel, SHADER_UNIFORM_FLOAT);
}

void LightingSystem::BeginShadowPass(Vector3 focus) {
    if (!m_shadowsReady) return;
    const float a = (m_timeOfDay - 0.25f) * 2.0f * PI;
    Vector3 d = sinf(a) >= 0.0f ? Vector3{ cosf(a), sinf(a), 0.5f } : Vector3{ -cosf(a), -sinf(a), 0.5f };
    d.y = fmaxf(d.y, 0.25f);                       // sole basso: ombre lunghe ma non infinite
    d = Vector3Normalize(d);

    Camera3D lc{};
    lc.target = focus;
    lc.position = Vector3Add(focus, Vector3Scale(d, 90.0f));
    lc.up = { 0.0f, 1.0f, 0.0f };
    lc.fovy = 70.0f;                               // in ortografica = larghezza dell'area coperta (blocchi)
    lc.projection = CAMERA_ORTHOGRAPHIC;

    int off = 0;
    SetShaderValue(m_lightingShader, m_locShadowsOn, &off, SHADER_UNIFORM_INT);
    rlSetClipPlanes(1.0, 250.0);
    BeginTextureMode(m_shadowRT);
    ClearBackground(WHITE);
    BeginMode3D(lc);
    m_lightVP = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
}

void LightingSystem::EndShadowPass() {
    if (!m_shadowsReady) return;
    EndMode3D();
    rlSetClipPlanes(RL_CULL_DISTANCE_NEAR, RL_CULL_DISTANCE_FAR);   // ripristina per la camera normale
    EndTextureMode();
    m_shadowsThisFrame = true;
}

} // namespace core
