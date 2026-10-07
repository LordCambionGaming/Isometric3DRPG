#include "entities/ArrowProjectile.h"
#include "core/Items.h"
#include "rendering/ItemVisuals.h"
#include <raymath.h>

ArrowProjectile::ArrowProjectile(Vector3 origin, Vector3 target)
    : m_position(origin), m_target(target) {
    m_direction = Vector3Normalize(Vector3Subtract(m_target, m_position));
}

bool ArrowProjectile::Update(float deltaTime) {
    const Vector3 remaining = Vector3Subtract(m_target, m_position);
    const float distance = Vector3Length(remaining);
    const float step = m_speed * deltaTime;
    if (distance <= step || distance <= 0.001f) {
        m_position = m_target;
        return false;
    }

    m_direction = Vector3Scale(remaining, 1.0f / distance);
    m_position = Vector3Add(m_position, Vector3Scale(m_direction, step));
    return true;
}

void ArrowProjectile::Render(ItemVisuals* visuals) const {
    if (!visuals || Vector3Length(m_direction) <= 0.001f) return;

    const Quaternion rotation = QuaternionFromVector3ToVector3(Vector3{ 0.0f, 0.0f, 1.0f }, m_direction);
    const Matrix arrowToWorld = MatrixMultiply(
        QuaternionToMatrix(rotation),
        MatrixTranslate(m_position.x, m_position.y, m_position.z));
    visuals->DrawInHand(core::items::Arrow, arrowToWorld);
}