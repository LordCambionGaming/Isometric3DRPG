#include "rendering/LightingSystem.h"
#include <cmath>
#include <algorithm>
#include <rlgl.h>

namespace core {

LightingSystem::LightingSystem() : m_timeOfDay(0.3f), m_shaderLoaded(false) {}

LightingSystem::~LightingSystem() {
    Unload();
}

void LightingSystem::Initialize() {
    Shader sh = LoadShader("assets/shaders/lighting.vs", "assets/shaders/lighting.fs");
    if (sh.id == 0 || sh.locs == nullptr) {
        TraceLog(LOG_ERROR, "[LIGHT] lighting.vs/.fs non si collegano: provo lighting_legacy");
        sh = LoadShader("assets/shaders/lighting_legacy.vs", "assets/shaders/lighting_legacy.fs");
    }
    if (sh.id == 0 || sh.locs == nullptr) {
        TraceLog(LOG_ERROR, "[LIGHT] nessuno shader di luce valido: uso quello di default di raylib");
        m_lightingShader = Shader{ rlGetShaderIdDefault(), rlGetShaderLocsDefault() };
        m_shaderLoaded = false;          // BeginLightingMode() non fa nulla, ma il gioco parte
        return;
    }
    m_lightingShader = sh;
    m_shaderLoaded = true;

    m_lightingShader.locs[SHADER_LOC_MATRIX_MVP]    = GetShaderLocation(m_lightingShader, "mvp");
    m_lightingShader.locs[SHADER_LOC_MATRIX_MODEL]  = GetShaderLocation(m_lightingShader, "matModel");
    m_lightingShader.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(m_lightingShader, "matNormal");
    
    
        
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
    if (m_timeOfDay < 0.2f || m_timeOfDay >= 0.8f) {
        return Color{ 10, 12, 40, 255 }; // Night
    } else if (m_timeOfDay < 0.3f) {
        float t = (m_timeOfDay - 0.2f) / 0.1f;
        return Color{ (unsigned char)(10 + t * 180), (unsigned char)(12 + t * 100), (unsigned char)(40 + t * 10), 255 };
    } else if (m_timeOfDay < 0.7f) {
        return Color{ 60, 120, 200, 255 }; // Day
    } else {
        float t = (m_timeOfDay - 0.7f) / 0.1f;
        return Color{ (unsigned char)(60 + t * 160), (unsigned char)(120 - t * 60), (unsigned char)(200 - t * 160), 255 };
    }
}

void LightingSystem::ClearLights() {
    m_lights.clear();
}

void LightingSystem::AddPointLight(Vector3 pos, Color color, float radius) {
    if (m_lights.size() < 8) { // MAX_LIGHTS in shader is 8
        m_lights.push_back({pos, color, radius});
    }
}

void LightingSystem::BeginLightingMode() {
    if (!m_shaderLoaded) return;
    
    float ambientIntensity = 0.0f;
    Vector3 ambientColor = {0.0f, 0.0f, 0.0f};
    Vector3 sunColor = {0.0f, 0.0f, 0.0f};
    
    // Sun rotates based on time of day. 0.25=Sunrise(East), 0.5=Noon(Top), 0.75=Sunset(West)
    // Map timeOfDay to angle: 0.25 -> 0, 0.5 -> PI/2, 0.75 -> PI
    float sunAngle = (m_timeOfDay - 0.25f) * 2.0f * PI;
    Vector3 sunDir = { cosf(sunAngle), sinf(sunAngle), 0.5f }; // slight angle in Z so faces get some light
    
    if (m_timeOfDay < 0.2f) { // Night
        ambientIntensity = 0.1f;
        ambientColor = {0.2f, 0.2f, 0.4f}; // Moonlight ambient
        sunColor = {0.05f, 0.05f, 0.15f}; // Weak moonlight (directional)
        sunDir = {0.0f, 1.0f, 0.5f}; // Moon from above
    } else if (m_timeOfDay < 0.3f) { // Sunrise
        float t = (m_timeOfDay - 0.2f) / 0.1f;
        ambientIntensity = 0.1f + t * 0.4f;
        ambientColor = {0.2f + t * 0.8f, 0.2f + t * 0.4f, 0.4f + t * 0.2f};
        sunColor = {t * 0.8f, t * 0.4f, 0.0f};
    } else if (m_timeOfDay < 0.7f) { // Day
        ambientIntensity = 0.4f;
        ambientColor = {0.6f, 0.6f, 0.6f}; 
        sunColor = {1.0f, 0.9f, 0.8f}; // Bright sun
    } else if (m_timeOfDay < 0.8f) { // Sunset
        float t = (m_timeOfDay - 0.7f) / 0.1f;
        ambientIntensity = 0.5f - t * 0.4f;
        ambientColor = {0.6f, 0.6f - t * 0.4f, 0.6f - t * 0.2f};
        sunColor = {1.0f - t * 0.5f, 0.9f - t * 0.6f, 0.8f - t * 0.8f}; 
    } else { // Night
        ambientIntensity = 0.1f;
        ambientColor = {0.2f, 0.2f, 0.4f};
        sunColor = {0.05f, 0.05f, 0.15f};
        sunDir = {0.0f, 1.0f, 0.5f};
    }
    
    SetShaderValue(m_lightingShader, m_locAmbientColor, &ambientColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(m_lightingShader, m_locAmbientIntensity, &ambientIntensity, SHADER_UNIFORM_FLOAT);
    SetShaderValue(m_lightingShader, m_locSunDirection, &sunDir, SHADER_UNIFORM_VEC3);
    SetShaderValue(m_lightingShader, m_locSunColor, &sunColor, SHADER_UNIFORM_VEC3);
    
    // Set lights
    int lightsCount = static_cast<int>(m_lights.size());
    SetShaderValue(m_lightingShader, m_locLightsCount, &lightsCount, SHADER_UNIFORM_INT);
    
    if (lightsCount > 0) {
        float positions[8 * 3];
        float colors[8 * 3];
        float radii[8];
        
        for (int i = 0; i < lightsCount; ++i) {
            positions[i*3 + 0] = m_lights[i].position.x;
            positions[i*3 + 1] = m_lights[i].position.y;
            positions[i*3 + 2] = m_lights[i].position.z;
            
            colors[i*3 + 0] = m_lights[i].color.r / 255.0f;
            colors[i*3 + 1] = m_lights[i].color.g / 255.0f;
            colors[i*3 + 2] = m_lights[i].color.b / 255.0f;
            
            radii[i] = m_lights[i].radius;
        }
        
        SetShaderValueV(m_lightingShader, m_locLightPositions, positions, SHADER_UNIFORM_VEC3, lightsCount);
        SetShaderValueV(m_lightingShader, m_locLightColors, colors, SHADER_UNIFORM_VEC3, lightsCount);
        SetShaderValueV(m_lightingShader, m_locLightRadii, radii, SHADER_UNIFORM_FLOAT, lightsCount);
    }

    Color sky = GetSkyColor();
Vector3 fogCol = { sky.r / 255.0f, sky.g / 255.0f, sky.b / 255.0f };
float fogDensity = 0.010f;   // 0 = spenta; prova 0.006 (lieve) .. 0.02 (fitta)
float exposure   = 1.05f;
SetShaderValue(m_lightingShader, m_locFogColor,   &fogCol,     SHADER_UNIFORM_VEC3);
SetShaderValue(m_lightingShader, m_locFogDensity, &fogDensity, SHADER_UNIFORM_FLOAT);
SetShaderValue(m_lightingShader, m_locExposure,   &exposure,   SHADER_UNIFORM_FLOAT);
    
    BeginShaderMode(m_lightingShader);
}

void LightingSystem::EndLightingMode() {
    if (!m_shaderLoaded) return;
    EndShaderMode();
}

} // namespace core
