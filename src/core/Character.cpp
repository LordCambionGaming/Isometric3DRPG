#include "core/Character.h"
#include <algorithm>

namespace core {

const char* ToString(Race race) {
    switch (race) {
        case Race::Human: return "Human";
    }
    return "?";
}

const char* ToString(CharacterClass cls) {
    switch (cls) {
        case CharacterClass::Warrior: return "Warrior";
        case CharacterClass::Ranger:  return "Ranger";
        case CharacterClass::Rogue:   return "Rogue";
    }
    return "?";
}

const char* ToString(Ability ability) {
    switch (ability) {
        case Ability::Str: return "STR";
        case Ability::Dex: return "DEX";
        case Ability::Con: return "CON";
        case Ability::Int: return "INT";
        case Ability::Wis: return "WIS";
        case Ability::Cha: return "CHA";
    }
    return "?";
}

int AbilityModifier(int score) {
    const int d = score - 10;
    return (d >= 0) ? d / 2 : -((-d + 1) / 2);   // floor(d / 2)
}

int AbilityScoreFromDice(const std::vector<int>& dice) {
    if (dice.empty()) return 0;
    int total = 0;
    for (int d : dice) total += d;
    return total - *std::min_element(dice.begin(), dice.end());
}

int RollAbilityScore() {
    return AbilityScoreFromDice(RollDice(4, 6).dice);
}

AbilityScores RollAbilityPool() {
    AbilityScores pool{};
    for (int& score : pool) score = RollAbilityScore();
    return pool;
}

AbilityScores StandardArray() {
    return { 15, 14, 13, 12, 10, 8 };
}

WeaponDef StartingWeapon(CharacterClass cls) {
    switch (cls) {
        case CharacterClass::Warrior: return { "Longsword", { 1, 8 } };
        case CharacterClass::Ranger:  return { "Bow",       { 1, 8 },8.0f };
        case CharacterClass::Rogue:   return { "Dagger",    { 1, 4 } };
    }
    return {};
}

Character CreateCharacter(CharacterClass cls, const AbilityScores& abilities) {
    Character c;
    c.race = Race::Human;
    c.characterClass = cls;
    c.level = 1;
    c.xp = 0;
    c.abilities = abilities;
    c.ownedWeapons.push_back(StartingWeapon(cls));
    c.equippedWeapon = -1;   // posseduta, non equipaggiata: si parte disarmati
    c.vitals.currentHP = c.MaxHP();
    return c;
}

} // namespace core