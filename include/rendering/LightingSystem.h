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
    //void UpdateTimeOfDay(float deltaTime);
    float GetTimeOfDay() const { return m_timeOfDay; }
    void SetTimeOfDay(float time) { m_timeOfDay = time; }
    Color GetSkyColor() const;

    void ClearLights();
    void AddPointLight(Vector3 pos, Color color, float radius);

    // Apply lighting to the shader before drawing 3D
    void BeginLightingMode();
    void EndLightingMode();

    Shader GetShader() const { return m_lightingShader; }

private:
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
};

} // namespace core
