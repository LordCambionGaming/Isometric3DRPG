#pragma once
#include "states/GameState.h"
#include "core/CombatSystem.h"
#include "core/Dice.h"
#include "ui/DiceRoller.h"
#include <raylib.h>
#include <vector>

class Mob;

// Dado "semplice" usato SOLO per i tiri che non sono un singolo d20 (1d4, 2d6, 3d20, ...).
struct VisualDice3D {
    Vector3 position{ 0.0f, 0.0f, 0.0f };
    Vector3 velocity{ 0.0f, 0.0f, 0.0f };
    Vector3 rotation{ 0.0f, 0.0f, 0.0f };
    Vector3 angularVelocity{ 0.0f, 0.0f, 0.0f };
    int sides = 20;
    int finalValue = 0;
    bool settled = false;
    Color color = WHITE;
};

// Un singolo d20 (attacco contro un Mob oppure 1d20 libero) usa la board gotica originale:
// feltro, guglie, d20 3D con ricamo dorato, lancio trascinando con il mouse.
// Qualsiasi altra espressione di dadi usa il vecchio percorso con i dadi 3D semplici.
class DiceRollState : public GameState {
public:
    // Tiro di attacco d20 (+ eventuale danno) contro un bersaglio
    DiceRollState(Game* game, Mob* target);

    // Tiro libero: 1d4, 2d6, 1d8, 1d10, 1d12, 1d20, ...
    DiceRollState(Game* game, core::DiceExpr expr);

    ~DiceRollState() override;

    void Enter() override;
    void Exit() override;
    void Update(float deltaTime) override;
    void Render() override;
    void HandleInput() override;

private:
    // --- board gotica (d20) ---
    void SetupGothicBoard();
    core::CombatRoll ThrowGothicDie();   // chiamata dal DiceRoller quando lanci il dado
    Rectangle Board() const;
    Rectangle Felt() const;
    void UpdateGothic(float deltaTime);
    void RenderGothic();
    void HandleInputGothic();

    // --- dadi semplici (tutto il resto) ---
    void InitDicePhysics();
    void UpdateSimple(float deltaTime);
    void RenderSimple();
    void RenderDie3D(const VisualDice3D& die);
    void HandleInputSimple();

    bool IsCombat() const { return m_isCombatRoll && m_target != nullptr; }

    Mob* m_target = nullptr;
    core::DiceExpr m_diceExpr{ 1, 20 };
    core::DiceResult m_diceResult{};

    core::AttackRollResult m_attackResult{};
    core::DamageRollResult m_damageResult{};
    bool m_isCombatRoll = false;
    bool m_attackRolled = false;    // l'attacco e' stato lanciato davvero
    bool m_damageRolled = false;
    bool m_damageApplied = false;
    bool m_combatDone = false;      // un attacco si lancia una volta sola

    // board gotica
    bool m_gothic = false;
    ui::DiceRoller m_dice;
    core::CombatRoll m_result{};
    bool m_showResult = false;
    float m_time = 0.0f;

    // dadi semplici
    std::vector<VisualDice3D> m_visualDice;
    Camera3D m_diceCamera{};
    float m_stateTimer = 0.0f;
    bool m_allSettled = false;
};