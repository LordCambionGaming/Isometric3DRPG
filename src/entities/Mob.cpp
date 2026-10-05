#include "entities/Mob.h"
#include <raymath.h>

Mob::Mob(std::string name, MobType type, Vector3 pos, int maxHP, int armorClass)
    : m_name(std::move(name))
    , m_type(type)
    , m_position(pos)
    , m_maxHP(maxHP)
    , m_currentHP(maxHP)
    , m_armorClass(armorClass)
    , m_color(GRAY)
{}

void Mob::Update(float dt) {
    // Logica di IA / Idle futuro
}

void Mob::Render(const Camera3D& camera) {
    if (!IsAlive()) return;

    // Rendering forma base Mob nel mondo 3D
    DrawCylinder(m_position, 0.4f, 0.4f, 1.2f, 8, m_color);
    DrawCylinderWires(m_position, 0.41f, 0.41f, 1.21f, 8, DARKGRAY);

    // Barra HP ed etichetta nome proiettate a schermo
    Vector3 barPos = { m_position.x, m_position.y + 1.5f, m_position.z };
    Vector2 screenPos = GetWorldToScreen(barPos, camera);

    if (screenPos.x > 0 && screenPos.x < GetScreenWidth() &&
        screenPos.y > 0 && screenPos.y < GetScreenHeight()) {
        
        DrawText(m_name.c_str(), (int)screenPos.x - MeasureText(m_name.c_str(), 12) / 2, (int)screenPos.y - 18, 12, WHITE);

        float barWidth = 40.0f;
        float barHeight = 5.0f;
        Rectangle bg = { screenPos.x - barWidth / 2.0f, screenPos.y, barWidth, barHeight };
        Rectangle fill = { screenPos.x - barWidth / 2.0f, screenPos.y, barWidth * GetHPFraction(), barHeight };

        DrawRectangleRec(bg, DARKGRAY);
        DrawRectangleRec(fill, m_type == MobType::Pacific ? GREEN : RED);
        DrawRectangleLinesEx(bg, 1.0f, BLACK);
    }
}

