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

ItemId StartingWeaponId(CharacterClass cls) {
    switch (cls) {
        case CharacterClass::Warrior: return items::Longsword;
        case CharacterClass::Ranger:  return items::Bow;
        case CharacterClass::Rogue:   return items::Dagger;
    }
    return items::Dagger;
}

Character CreateCharacter(CharacterClass cls, const AbilityScores& abilities) {
    Character c;
    c.race = Race::Human;
    c.characterClass = cls;
    c.level = 1;
    c.xp = 0;
    c.abilities = abilities;
    c.inventory.push_back({ StartingWeaponId(cls), 1 });
    // equipment starts empty (all slots kNoItem by default)
    c.vitals.currentHP = c.MaxHP();
    return c;
}

bool Character::CanEquip(ItemId id, EquipSlot slot) const {
    if (id == kNoItem) return true;
    const ItemDef* def = GetItem(id);
    if (!def) return false;

    // Check slot compatibility
    bool slotMatch = false;
    if (def->validSlot == EquipSlot::AnyHand) {
        slotMatch = (slot == EquipSlot::RightHand || slot == EquipSlot::LeftHand);
    } else {
        slotMatch = (def->validSlot == slot);
    }
    if (!slotMatch) return false;

    // Check two-handed constraints
    if (def->category == ItemCategory::Weapon && (def->weapon.grip == WeaponGrip::TwoHanded || def->weapon.grip == WeaponGrip::Bow)) {
        if (slot == EquipSlot::RightHand && equipment.leftHand != kNoItem) return false;
        if (slot == EquipSlot::LeftHand && equipment.rightHand != kNoItem) return false;
    }

    // Check if we are equipping something in one hand while the other has a two-handed weapon
    if (slot == EquipSlot::LeftHand && equipment.rightHand != kNoItem) {
        const ItemDef* mainWep = GetItem(equipment.rightHand);
        if (mainWep && mainWep->category == ItemCategory::Weapon && 
            (mainWep->weapon.grip == WeaponGrip::TwoHanded || mainWep->weapon.grip == WeaponGrip::Bow)) {
            return false;
        }
    }
    if (slot == EquipSlot::RightHand && equipment.leftHand != kNoItem) {
        const ItemDef* offWep = GetItem(equipment.leftHand);
        if (offWep && offWep->category == ItemCategory::Weapon && 
            (offWep->weapon.grip == WeaponGrip::TwoHanded || offWep->weapon.grip == WeaponGrip::Bow)) {
            return false;
        }
    }

    return true;
}

WeaponDef Character::ActiveWeapon() const {
    WeaponDef w = ResolveWeapon(equipment.rightHand);
    if (w.isVersatile && equipment.leftHand == kNoItem) {
        w.damage = w.versatileDamage;
        w.grip = WeaponGrip::TwoHanded;
    }
    return w;
}

void Character::AddItem(ItemId id, int count) {
    if (count <= 0) return;
    for (auto& stack : inventory) {
        if (stack.id == id) {
            stack.count += count;
            return;
        }
    }
    inventory.push_back({ id, count });
}

} // namespace core