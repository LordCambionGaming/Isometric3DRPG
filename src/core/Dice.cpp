#include "core/Dice.h"
#include <random>

namespace core {

namespace {

std::mt19937& Rng() {
    static std::mt19937 gen{ std::random_device{}() };
    return gen;
}

} // namespace

void SeedDice(unsigned int seed) {
    Rng().seed(seed);
}

int RollDie(int sides) {
    if (sides < 1) return 0;
    std::uniform_int_distribution<int> dist(1, sides);
    return dist(Rng());
}

DiceResult RollDice(int count, int sides) {
    DiceResult result;
    if (count < 1 || sides < 1) return result;
    result.dice.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        const int v = RollDie(sides);
        result.dice.push_back(v);
        result.total += v;
    }
    return result;
}

DiceResult RollDice(const DiceExpr& expr) {
    return RollDice(expr.count, expr.sides);
}

std::string ToString(const DiceExpr& expr) {
    return std::to_string(expr.count) + "d" + std::to_string(expr.sides);
}

} // namespace core
