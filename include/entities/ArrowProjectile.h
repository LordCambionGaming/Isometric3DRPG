#pragma once

#include <raylib.h>

class ItemVisuals;

class ArrowProjectile {
public:
    ArrowProjectile(Vector3 origin, Vector3 target);

    bool Update(float deltaTime);
    void Render(ItemVisuals* visuals) const;

private:
    Vector3 m_position{};
    Vector3 m_target{};
    Vector3 m_direction{};
    float m_speed = 14.0f;
};