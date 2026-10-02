#pragma once
#include <memory>
#include <string>

namespace core {

enum class CombatResult {
    CriticalFailure,  // 1 naturale
    Normal,           // 2-19
    CriticalSuccess   // 20 naturale
};

struct CombatRoll {
    int rollValue;
    CombatResult result;
    std::string description;
};

class CombatSystem {
public:
    CombatSystem();
    ~CombatSystem() = default;

    void Initialize();
    void Reset();

    CombatRoll RollD20(int difficultyModifier = 0);
    CombatResult GetLastResult() const { return m_lastResult; }
    int GetLastRollValue() const { return m_lastRollValue; }
    std::string GetLastDescription() const { return m_lastDescription; }

    CombatRoll TestCombatRoll(int difficultyModifier = 0);

private:
    CombatResult m_lastResult;
    int m_lastRollValue;
    std::string m_lastDescription;
};

} // namespace core