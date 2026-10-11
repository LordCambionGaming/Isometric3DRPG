#include "rendering/Camera3DController.h"
#include "core/KeyBindings.h"
#include <raymath.h>
#include <iostream>

Camera3DController::Camera3DController()
    : m_zoom(1.0f)
    , m_rotation(0.0f)
    , m_distance(DEFAULT_DISTANCE)
    , m_height(DEFAULT_HEIGHT)
    , m_followTarget({ 0.0f, 0.0f, 0.0f })
    , m_followOffset({ 0.0f, 0.0f, 0.0f })
    , m_followActive(false)
    , m_mouseDragging(false)
    , m_lastMousePosition({0.0f, 0.0f})
{
}

void Camera3DController::Initialize() {
    // Initialize camera
    m_camera.position = { 0.0f, m_height, m_distance };
    m_camera.target = { 0.0f, 0.0f, 0.0f };
    m_camera.up = { 0.0f, 1.0f, 0.0f };
    m_camera.fovy = 45.0f;
    m_camera.projection = CAMERA_PERSPECTIVE;

    SetIsometricView();
}

void Camera3DController::Update(float deltaTime) {
    UpdateCameraPosition();
}

void Camera3DController::HandleInput() {
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        if (!m_mouseDragging) {
            m_mouseDragging = true;
            m_lastMousePosition = GetMousePosition();
        } else {
            Vector2 currentMousePos = GetMousePosition();
            Vector2 delta = {
                currentMousePos.x - m_lastMousePosition.x,
                currentMousePos.y - m_lastMousePosition.y
            };

            Pan(Vector2Scale(delta, -0.01f));
            m_lastMousePosition = currentMousePos;
        }
    } else {
        m_mouseDragging = false;
    }

    // Mouse wheel for zoom
    float wheelMove = GetMouseWheelMove();
    if (wheelMove != 0.0f) {
        Zoom(1.0f + wheelMove * ZOOM_SPEED);
    }

    // Arrow keys for rotation
    if (core::KeyBindings::Down(core::KeyBindings::Action::Left)) {
        Rotate(-ROTATION_SPEED * GetFrameTime());
    }
    if (core::KeyBindings::Down(core::KeyBindings::Action::Right)) {
        Rotate(ROTATION_SPEED * GetFrameTime());
    }

    // WASD for camera movement
    Vector2 moveInput = {0.0f, 0.0f};
    if (IsKeyDown(KEY_W)) moveInput.y += 1.0f;
    if (IsKeyDown(KEY_S)) moveInput.y -= 1.0f;
    if (IsKeyDown(KEY_A)) moveInput.x -= 1.0f;
    if (IsKeyDown(KEY_D)) moveInput.x += 1.0f;

    if (Vector2Length(moveInput) > 0.0f) {
        moveInput = Vector2Scale(Vector2Normalize(moveInput), PAN_SPEED * GetFrameTime());
        Pan(moveInput);
    }

    // Reset camera
    if (IsKeyPressed(KEY_HOME)) {
        SetIsometricView();
    }
}

void Camera3DController::Rotate(float angle) {
    m_rotation += angle;
    UpdateCameraPosition();
}

void Camera3DController::Zoom(float factor) {
    m_zoom *= factor;
    ClampZoom();
    UpdateCameraPosition();
}

void Camera3DController::Pan(Vector2 delta) {
    // Convert screen-space pan to world-space movement
    Vector3 right = Vector3Normalize(Vector3CrossProduct(
        Vector3Subtract(m_camera.target, m_camera.position), m_camera.up));
    Vector3 forward = Vector3Normalize(Vector3CrossProduct(m_camera.up, right));

    Vector3 movement = Vector3Add(
        Vector3Scale(right, delta.x),
        Vector3Scale(forward, delta.y)
    );

    if (m_followActive) m_followOffset = Vector3Add(m_followOffset, movement);
    m_camera.target = Vector3Add(m_camera.target, movement);
    UpdateCameraPosition();
}

void Camera3DController::FollowTarget(Vector3 target) {
    if (!m_followActive) {
        m_followOffset = Vector3Subtract(m_camera.target, target);
        m_followActive = true;
    }
    m_followTarget = target;
    m_camera.target = Vector3Add(target, m_followOffset);
    UpdateCameraPosition();
}

void Camera3DController::SetTarget(Vector3 target) {
    m_camera.target = target;
    if (m_followActive) m_followOffset = Vector3Subtract(target, m_followTarget);
    UpdateCameraPosition();
}

void Camera3DController::CenterOn(Vector3 target) {
    m_followTarget = target;
    m_followOffset = { 0.0f, 0.0f, 0.0f };
    m_followActive = true;
    m_camera.target = target;
    UpdateCameraPosition();
}

void Camera3DController::SetView(Vector3 target, float rotation, float zoom) {
    m_followActive = false;
    m_followOffset = { 0.0f, 0.0f, 0.0f };
    m_rotation = rotation;
    m_zoom = zoom;
    ClampZoom();
    m_camera.target = target;
    UpdateCameraPosition();
}

void Camera3DController::SetIsometricView() {
    m_rotation = 45.0f * DEG2RAD;  // 45 degrees for isometric view
    m_zoom = 1.0f;
    m_followActive = false;
    m_followOffset = { 0.0f, 0.0f, 0.0f };
    m_camera.target = { 0.0f, 0.0f, 0.0f };
    UpdateCameraPosition();
}

void Camera3DController::UpdateCameraPosition() {
    float adjustedDistance = m_distance / m_zoom;

    // Calculate position based on rotation around target
    float x = cosf(m_rotation) * adjustedDistance;
    float z = sinf(m_rotation) * adjustedDistance;

    m_camera.position = {
        m_camera.target.x + x,
        m_camera.target.y + m_height / m_zoom,
        m_camera.target.z + z
    };
}

void Camera3DController::ClampZoom() {
    if (m_zoom < MIN_ZOOM) m_zoom = MIN_ZOOM;
    if (m_zoom > MAX_ZOOM) m_zoom = MAX_ZOOM;
}