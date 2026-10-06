#pragma once
#include <raylib.h>
#include <string>
#include <algorithm>

enum class MobType {
    Enemy,   // Ostile: attaccabile
    Animal,  // Animale: neutrale/ostile, attaccabile
    Pacific  // NPC Pacifico: non attaccabile
};

class Mob {
public:
    Mob(std::string name, MobType type, Vector3 pos, int maxHP, int armorClass);
    virtual ~Mob() = default;

    virtual void Update(float dt);
    virtual void Render(const Camera3D& camera);
    virtual void RenderOverlay(const Camera3D& camera); 

    const std::string& GetName() const { return m_name; }
    MobType GetType() const { return m_type; }
    Vector3 GetPosition() const { return m_position; }
    void SetPosition(Vector3 pos) { m_position = pos; }

    int GetCurrentHP() const { return m_currentHP; }
    int GetMaxHP() const { return m_maxHP; }
    int GetArmorClass() const { return m_armorClass; }
    float GetHPFraction() const { return m_maxHP > 0 ? (float)m_currentHP / (float)m_maxHP : 0.0f; }

    bool IsAlive() const { return m_currentHP > 0; }
    bool IsAttackable() const { return m_type != MobType::Pacific && IsAlive(); }
    void SetHovered(bool hovered) { m_hovered = hovered; }

    virtual void TakeDamage(int damage) {
        m_currentHP = std::max(0, m_currentHP - damage);
    }

    virtual void Heal(int amount) {
        m_currentHP = std::min(m_maxHP, m_currentHP + amount);
    }
    BoundingBox GetBounds() const {
    return { { m_position.x - 0.4f, m_position.y,        m_position.z - 0.4f },
             { m_position.x + 0.4f, m_position.y + 1.2f, m_position.z + 0.4f } };
    }

protected:
    std::string m_name;
    MobType m_type;
    Vector3 m_position;
    int m_maxHP;
    int m_currentHP;
    int m_armorClass;
    Color m_color;
    bool m_hovered = false;
};

// --- Nemico Ostile ---
class Enemy : public Mob {
public:
    Enemy(std::string name, Vector3 pos, int maxHP = 15, int armorClass = 12)
        : Mob(std::move(name), MobType::Enemy, pos, maxHP, armorClass) {
        m_color = RED;
    }
};

// --- Animale ---
class Animal : public Mob {
public:
    Animal(std::string name, Vector3 pos, int maxHP = 8, int armorClass = 10)
        : Mob(std::move(name), MobType::Animal, pos, maxHP, armorClass) {
        m_color = ORANGE;
    }
};

// --- NPC Pacifico ---
class PacificMob : public Mob {
public:
    PacificMob(std::string name, Vector3 pos)
        : Mob(std::move(name), MobType::Pacific, pos, 30, 10) {
        m_color = GREEN;
    }
};