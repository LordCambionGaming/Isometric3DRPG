#pragma once
#include "states/GameState.h"
#include "core/CombatSystem.h"
#include "core/Dice.h"
#include "ui/DiceRoller.h"
#include <raylib.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class Mob;

// Dado "semplice": serve solo per i tiri che la board gotica non sa mostrare (facce diverse da 4/6/8/10/12/20).
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

// Per usare la board come passaggio di un'altra schermata (per esempio la Character Creation):
// al posto di tornare al PlayingState, chiude restituendo lo stato scelto da chi l'ha aperta.
struct DiceRollOptions {
    // Dove andare quando chiudi la board (INVIO / ESC). Riceve i dadi lanciati e se il lancio e' stato
    // completato: chiudendo a meta' i dadi sono vuoti e completed e' false.
    std::function<std::unique_ptr<GameState>(const std::vector<int>& dice, bool completed)> onFinish;
    // Testo del risultato finale al posto di "Risultato NdX: totale" (facoltativo)
    std::function<std::string(const std::vector<int>& dice)> summary;
    bool opaqueBackdrop = false;   // sfondo pieno invece del mondo oscurato (quando dietro non c'e' il mondo)
    bool lockWhenDone = true;      // a lancio completato i dadi non si possono rilanciare
};

// Board gotica: feltro, guglie, dado 3D con ricamo dorato, lancio trascinando col mouse.
//  - Attacco (Mob*): lanci il d20; se COLPISCI devi lanciare anche il dado del danno
//    (con un colpo critico i dadi del danno sono il doppio, e li lanci uno dopo l'altro).
//  - Tiro libero (DiceExpr): lanci d4/d6/d8/d10/d12/d20, uno per volta fino a NdX.
class DiceRollState : public GameState {
public:
    // Tiro di attacco d20 contro un bersaglio (+ dado del danno se colpisce)
    DiceRollState(Game* game, Mob* target);

    // Tiro libero: 1d4, 2d6, 1d8, 1d10, 1d12, 1d20, ...
    DiceRollState(Game* game, core::DiceExpr expr);

    // Tiro libero aperto da un'altra schermata, che decide dove tornare (vedi DiceRollOptions)
    DiceRollState(Game* game, core::DiceExpr expr, DiceRollOptions options);

    ~DiceRollState() override;

    void Enter() override;
    void Exit() override;
    void Update(float deltaTime) override;
    void Render() override;
    void HandleInput() override;

private:
    enum class Phase { Attack, Damage, Free };      // a cosa serve il dado che hai davanti
    enum class Pending { None, ReArm, ToDamage };   // piccola pausa tra un lancio e il successivo

    // --- board gotica ---
    void SetupGothicBoard();
    core::CombatRoll ThrowGothicDie();              // chiamata dal DiceRoller quando lanci il dado
    void OnThrowResolved(const core::CombatRoll& r);
    void BeginPool(const core::DiceExpr& pool);     // mette sul feltro il dado giusto, pronto da lanciare
    core::DiceExpr DamagePool() const;
    void FinishDamage();
    Rectangle Board() const;
    Rectangle Felt() const;
    void UpdateGothic(float deltaTime);
    void RenderGothic();
    void HandleInputGothic();

    // --- dadi semplici ---
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
    DiceRollOptions m_opts;
    bool m_attackRolled = false;    // l'attacco e' stato lanciato davvero
    bool m_damageRolled = false;    // tutti i dadi del danno sono stati lanciati
    bool m_damageApplied = false;
    bool m_combatDone = false;      // un attacco (con il suo danno) si lancia una volta sola

    // board gotica
    bool m_gothic = false;
    Phase m_phase = Phase::Free;
    Pending m_pending = Pending::None;
    float m_pendingTimer = 0.0f;
    core::DiceExpr m_pool{ 1, 20 };          // dadi del lancio in corso
    std::vector<int> m_poolValues;           // quelli gia' lanciati
    bool m_poolDone = false;
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