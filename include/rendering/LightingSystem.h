#pragma once
#include <raylib.h>
#include <vector>

namespace core {

struct PointLight {
    Vector3 position;
    Color color;
    float radius;
};

class LightingSystem {
public:
    LightingSystem();
    ~LightingSystem();

    void Initialize();
    void Unload();

    void UpdateTimeOfDay(float deltaTime);
    float GetTimeOfDay() const { return m_timeOfDay; }
    void SetTimeOfDay(float time) { m_timeOfDay = time; }
    Color GetSkyColor() const;

    void ClearLights();
    void AddPointLight(Vector3 pos, Color color, float radius);

    // Apply lighting to the shader before drawing 3D
    void BeginLightingMode(Vector3 cameraPos);
    void AddTorch(Vector3 pos, float radius = 7.0f, float strength = 1.0f);
    void EndLightingMode();

    Shader GetShader() const { return m_lightingShader; }

    void InitShadows(int size = 2048);
void BeginShadowPass(Vector3 focus);
void EndShadowPass();
private:

    RenderTexture2D m_shadowRT{};
Matrix m_lightVP{};
bool m_shadowsReady = false, m_shadowsThisFrame = false;
int m_shadowSize = 2048;
int m_locLightVP = -1, m_locShadowMap = -1, m_locShadowsOn = -1, m_locShadowTexel = -1;
    float m_timeOfDay; // 0.0 to 1.0 (0.0 = midnight, 0.5 = noon)
    Shader m_lightingShader;
    bool m_shaderLoaded;

    std::vector<PointLight> m_lights;

    // Uniform locations
    int m_locAmbientColor;
    int m_locAmbientIntensity;
    int m_locLightsCount;
    int m_locLightPositions;
    int m_locLightColors;
    int m_locLightRadii;
    int m_locSunDirection;
    int m_locSunColor;
    int m_locFogColor = -1;
    int m_locFogDensity = -1;
    int m_locExposure = -1;
    int m_locViewPos = -1;

};

} // namespace core
