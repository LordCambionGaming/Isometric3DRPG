#include "core/CombatSystem.h"
#include "core/Dice.h"
#include "utils/Logger.h"
#include <random>
#include <sstream>
#include <iostream>

namespace core {

CombatSystem::CombatSystem() : m_lastResult(CombatResult::Normal), m_lastRollValue(0) {}

void CombatSystem::Initialize() {
    Reset();
}

void CombatSystem::Reset() {
    m_lastResult = CombatResult::Normal;
    m_lastRollValue = 0;
    m_lastDescription.clear();
}

CombatRoll CombatSystem::RollD20(int difficultyModifier) {
    // Fonte unica di casualita': core/Dice (la stessa usata dalla Character Creation e dai futuri tiri NdX)
    int roll = RollDie(20);
    CombatRoll result;
    result.rollValue = roll;

    // Calcola il risultato considerando il modificatore di difficoltà
    int total = roll + difficultyModifier;

    if (roll == 1) {
        result.result = CombatResult::CriticalFailure;
        result.description = "CRITICAL FAILURE! Hai tirato un 1 naturale.";
    } else if (roll == 20) {
        result.result = CombatResult::CriticalSuccess;
        result.description = "CRITICAL SUCCESS! Hai tirato un 20 naturale.";
    } else {
        result.result = CombatResult::Normal;
        result.description = "Tiro normale: " + std::to_string(roll);
    }

    // Aggiorna gli ultimi risultati
    m_lastResult = result.result;
    m_lastRollValue = roll;
    m_lastDescription = result.description;

    return result;
}

CombatRoll CombatSystem::TestCombatRoll(int difficultyModifier) {
    CombatRoll roll = RollD20(difficultyModifier);

    // Stampa il risultato per debug/test (ora su file log invece che console)
    switch (roll.result) {
        case CombatResult::CriticalFailure:
            Logger::LogError("RISULTATO: CRITICAL FAILURE");
            break;
        case CombatResult::CriticalSuccess:
            Logger::LogInfo("RISULTATO: CRITICAL SUCCESS");
            break;
        case CombatResult::Normal:
            // Nessun output per i casi normali
            break;
    }

    Logger::LogDebug(roll.description);

    return roll;
}

} // namespace core
