#pragma once

#include <raylib.h>

class Camera3DController {
public:
    Camera3DController();
    ~Camera3DController() = default;

    void Initialize();
    void Update(float deltaTime);
    void FollowTarget(Vector3 target); // per seguire il player
    void HandleInput();

    const Camera3D& GetCamera() const { return m_camera; }
    Camera3D& GetCamera() { return m_camera; }

    void SetPosition(Vector3 position) { m_camera.position = position; }
    void SetTarget(Vector3 target);
    void CenterOn(Vector3 target); // centra la camera sul target, azzera l'offset e attiva il follow

    // Isometric camera controls
    void Rotate(float angle);
    void Zoom(float factor);
    void Pan(Vector2 delta);
    void SetIsometricView();
    void SetView(Vector3 target, float rotation, float zoom);

    // Camera properties
    float GetZoom() const { return m_zoom; }
    float GetRotation() const { return m_rotation; }
    Vector3 GetPosition() const { return m_camera.position; }
    Vector3 GetTarget() const { return m_camera.target; }

private:
    void UpdateCameraPosition();
    void ClampZoom();

    Camera3D m_camera;
    float m_zoom;
    float m_rotation;
    float m_distance;
    float m_height;
    Vector3 m_followTarget{};
    Vector3 m_followOffset{};
    bool m_followActive = false;

    // Input state
    bool m_mouseDragging;
    Vector2 m_lastMousePosition;

    // Configuration
    static constexpr float MIN_ZOOM = 0.5f;
    static constexpr float MAX_ZOOM = 3.0f;
    static constexpr float ZOOM_SPEED = 0.1f;
    static constexpr float ROTATION_SPEED = 2.0f;
    static constexpr float PAN_SPEED = 10.0f;
    static constexpr float DEFAULT_DISTANCE = 20.0f;
    static constexpr float DEFAULT_HEIGHT = 15.0f;
};