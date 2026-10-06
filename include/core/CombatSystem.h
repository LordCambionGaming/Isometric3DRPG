#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include "core/Dice.h"

namespace core {

enum class CombatResult {
    CriticalFailure,
    Normal,
    CriticalSuccess
};

struct CombatRoll {
    int rollValue = 0;
    CombatResult result = CombatResult::Normal;
    std::string description;
};

struct AttackRollResult {
    int d20Roll = 0;
    int totalAttack = 0;
    int targetAC = 0;
    bool isHit = false;
    CombatResult resultType = CombatResult::Normal;
    std::string log;
};

struct DamageRollResult {
    DiceExpr weaponDice{ 1, 6 };
    int diceTotal = 0;
    int modifier = 0;
    int totalDamage = 0;
    std::string log;
};

class CombatSystem {
public:
    CombatSystem() = default;

    void Initialize() {}
    void Reset() {}

    CombatRoll TestCombatRoll(int modifier = 0) {
        int d20 = RollDie(20);
        CombatRoll roll;
        roll.rollValue = d20;
        if (d20 == 1) roll.result = CombatResult::CriticalFailure;
        else if (d20 == 20) roll.result = CombatResult::CriticalSuccess;
        else roll.result = CombatResult::Normal;
        roll.description = "Tiro D20: " + std::to_string(d20 + modifier);
        return roll;
    }

    AttackRollResult RollAttack(int attackBonus, int targetAC) {
        int d20 = RollDie(20);
        AttackRollResult res;
        res.d20Roll = d20;
        res.totalAttack = d20 + attackBonus;
        res.targetAC = targetAC;

        if (d20 == 1) {
            res.resultType = CombatResult::CriticalFailure;
            res.isHit = false;
            res.log = "Fallimento Critico! (1 Naturale)";
        } else if (d20 == 20) {
            res.resultType = CombatResult::CriticalSuccess;
            res.isHit = true;
            res.log = "Colpo Critico! (20 Naturale)";
        } else {
            res.resultType = CombatResult::Normal;
            res.isHit = (res.totalAttack >= targetAC);
            res.log = res.isHit ? "Colpito!" : "Mancato!";
        }
        return res;
    }

    // Esito del danno a partire da dadi GIA' lanciati (per esempio lanciati a mano sulla board dei dadi).
    // E' l'unico punto che applica la formula del danno: RollDamage qui sotto la usa a sua volta.
    DamageRollResult ResolveDamage(const DiceExpr& weaponDice, const std::vector<int>& rolls, int statModifier) {
        int sum = 0;
        for (int r : rolls) sum += r;

        DamageRollResult res;
        res.weaponDice = weaponDice;
        res.diceTotal = sum;
        res.modifier = statModifier;
        res.totalDamage = std::max(1, sum + statModifier);
        res.log = "Danno: " + std::to_string(res.totalDamage) +
                  " (" + ToString(weaponDice) + " + " + std::to_string(statModifier) + ")";
        return res;
    }

    // Tira i dadi del danno al posto del giocatore (colpo critico = dadi raddoppiati).
    DamageRollResult RollDamage(const DiceExpr& weaponDice, int statModifier, bool isCrit = false) {
        int diceCount = isCrit ? weaponDice.count * 2 : weaponDice.count;
        DiceResult diceRes = RollDice(diceCount, weaponDice.sides);
        return ResolveDamage(weaponDice, diceRes.dice, statModifier);
    }
};

} // namespace core