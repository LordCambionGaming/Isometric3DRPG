#pragma once

#include <raylib.h>

class Player;

class HUD {
public:
    HUD() = default;
    ~HUD() = default;

    // HUD is rendered after the 3D pass, so every primitive here is screen-space.
    void Render(const Player& player, const Camera3D& camera);

    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }

private:
    // playerPosition: feet (used for the in/out-of-view decision, unchanged).
    // headAnchor: world point above the head where the on-screen chevron is drawn.
    void DrawPlayerIndicator(const Vector3& playerPosition, const Vector3& headAnchor, const Camera3D& camera) const;

    bool m_enabled = true;
};