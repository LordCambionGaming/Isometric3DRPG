#include "core/CombatSystem.h"
#include <random>
#include <sstream>

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
    // Generatore di numeri casuali
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(1, 20);

    int roll = dist(gen);
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

    // Stampa il risultato per debug/test (mantenuto solo se necessario)
    switch (roll.result) {
        case CombatResult::CriticalFailure:
            std::cout << "RISULTATO: CRITICAL FAILURE" << std::endl;
            break;
        case CombatResult::CriticalSuccess:
            std::cout << "RISULTATO: CRITICAL SUCCESS" << std::endl;
            break;
        case CombatResult::Normal:
            // Nessun output per i casi normali
            break;
    }

    std::cout << roll.description << std::endl;
    std::cout << "-----------------------------------\n" << std::endl;

    return roll;
}

} // namespace core
