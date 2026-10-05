#pragma once
#include <string>
#include <vector>

namespace core {

// Primitive riutilizzabili per qualsiasi tiro: d4/d6/d8/d10/d12/d20 e NdX.
// E' l'unica fonte di casualita' del gioco: CombatSystem::RollD20 e la Character Creation
// pescano entrambi da qui. Non e' un sistema parallelo a CombatSystem/DiceRoller: la board 3D
// (DiceRoller / DiceRollState) continua a mostrare il d20 prodotto da CombatSystem.

// "NdX": `count` dadi a `sides` facce (es. 1d8 -> { 1, 8 }).
struct DiceExpr {
    int count = 1;
    int sides = 20;
};

struct DiceResult {
    std::vector<int> dice;  // ogni dado, nell'ordine di lancio
    int total = 0;          // somma di tutti i dadi
};

int RollDie(int sides);                       // uniforme in [1, sides]; 0 se sides < 1
DiceResult RollDice(int count, int sides);    // NdX; risultato vuoto se count < 1 o sides < 1
DiceResult RollDice(const DiceExpr& expr);
std::string ToString(const DiceExpr& expr);   // "1d8"

// Per test/riproducibilita': senza chiamarla il generatore parte da std::random_device.
void SeedDice(unsigned int seed);

} // namespace core
