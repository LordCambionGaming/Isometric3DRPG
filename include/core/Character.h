#pragma once
#include <array>
#include <cstddef>
#include <vector>
#include "core/Dice.h"

namespace core {

enum class Race { Human };
enum class CharacterClass { Warrior, Ranger, Rogue };
enum class Ability { Str, Dex, Con, Int, Wis, Cha };

constexpr int kCharacterClassCount = 3;
constexpr int kAbilityCount = 6;

using AbilityScores = std::array<int, kAbilityCount>;   // indicizzato con Ability

const char* ToString(Race race);
const char* ToString(CharacterClass cls);
const char* ToString(Ability ability);                   // "STR", "DEX", ...

// Modificatore D&D: floor((score - 10) / 2). Divisione per difetto, non troncata:
// 9 -> -1 (con la divisione intera di C++ verrebbe 0).
int AbilityModifier(int score);

// 4d6 scartando il dado piu' basso (3..18), costruito su core::RollDice.
int RollAbilityScore();
// I sei punteggi della Character Creation (sei tiri indipendenti, nell'ordine di lancio).
AbilityScores RollAbilityPool();

struct WeaponDef {
    const char* name = "";
    DiceExpr damage{ 0, 0 };
};

// Warrior -> Longsword 1d8, Ranger -> Bow 1d8, Rogue -> Dagger 1d4.
WeaponDef StartingWeapon(CharacterClass cls);

struct Character {
    Race race = Race::Human;
    CharacterClass characterClass = CharacterClass::Warrior;
    int level = 1;
    int xp = 0;
    AbilityScores abilities{};                 // valorizzato da CreateCharacter()
    std::vector<WeaponDef> ownedWeapons;       // armi possedute
    int equippedWeapon = -1;                   // indice in ownedWeapons; -1 = disarmato

    int Score(Ability a) const { return abilities[static_cast<std::size_t>(a)]; }
    int Modifier(Ability a) const { return AbilityModifier(Score(a)); }
    bool HasEquippedWeapon() const {
        return equippedWeapon >= 0 && equippedWeapon < static_cast<int>(ownedWeapons.size());
    }
};

// Human, livello 1, XP 0, possiede l'arma iniziale della classe ma non la equipaggia.
Character CreateCharacter(CharacterClass cls, const AbilityScores& abilities);

} // namespace core
