#pragma once
#include <algorithm>
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

// Punteggio da quattro d6 GIA' lanciati (per esempio sulla board dei dadi): somma meno il piu' basso (3..18).
int AbilityScoreFromDice(const std::vector<int>& dice);
// 4d6 scartando il dado piu' basso (3..18), costruito su core::RollDice.
int RollAbilityScore();
// I sei punteggi della Character Creation (sei tiri indipendenti, nell'ordine di lancio).
AbilityScores RollAbilityPool();
// Alternativa D&D 5e al tirare i dadi: l'array standard 15, 14, 13, 12, 10, 8.
AbilityScores StandardArray();

struct WeaponDef {
    const char* name = "";
    DiceExpr damage{ 1, 8 };  // Default: 1d8
    float range = 1.5f;       // Portata in metri (1.5m = mischia, 8m = distanza)
};

WeaponDef StartingWeapon(CharacterClass cls);



struct Vitals {
    int currentHP = 0;
    int tempHP = 0;        // punti ferita temporanei (D&D 5e)
    int currentMP = 0;     // mana (per il futuro)

    bool IsAlive() const { return currentHP > 0; }
};

struct Character {
    Race race = Race::Human;
    CharacterClass characterClass = CharacterClass::Warrior;
    int level = 1;
    int xp = 0;
    AbilityScores abilities{};                 // valorizzato da CreateCharacter()
    std::vector<WeaponDef> ownedWeapons;       // armi possedute
    int equippedWeapon = -1;                   // indice in ownedWeapons; -1 = disarmato

    Vitals vitals{};
    int HitDie()const {
        switch (characterClass) {
            case CharacterClass::Rogue: return 8;
            case CharacterClass::Warrior:
            case CharacterClass::Ranger:
            default: return 10;
        }
    }

    int MaxHP() const {
        const int die = HitDie();
        const int modCon = Modifier(Ability::Con);
        int hp = die + modCon;                      // livello 1
        for (int l = 2; l <= level; ++l)
            hp += (die / 2 + 1) + modCon;           // livelli successivi (media D&D 5e)
        return std::max(1, hp);                     // mai < 1
    }

        int HPPercent() const {
        const int max = MaxHP();
        return (max > 0) ? (vitals.currentHP * 100) / max : 0;
    }

    float HPFraction() const {
        const int max = MaxHP();
        return (max > 0) ? static_cast<float>(vitals.currentHP) / static_cast<float>(max) : 0.0f;
    }

    int Score(Ability a) const { return abilities[static_cast<std::size_t>(a)]; }
    int Modifier(Ability a) const { return AbilityModifier(Score(a)); }
    bool HasEquippedWeapon() const {
        return equippedWeapon >= 0 && equippedWeapon < static_cast<int>(ownedWeapons.size());
    }
};

// Human, livello 1, XP 0, possiede l'arma iniziale della classe ma non la equipaggia.
Character CreateCharacter(CharacterClass cls, const AbilityScores& abilities);

} // namespace core