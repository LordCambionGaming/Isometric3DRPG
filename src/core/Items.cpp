#include "core/Items.h"
#include <cstring>

namespace core {

const char* ToString(ItemCategory category) {
    switch (category) {
        case ItemCategory::Weapon:     return "Arma";
        case ItemCategory::Armor:      return "Armatura";
        case ItemCategory::Consumable: return "Consumabile";
        case ItemCategory::Misc:       return "Varie";
    }
    return "?";
}

const char* ToString(WeaponGrip grip) {
    switch (grip) {
        case WeaponGrip::Unarmed:   return "Pugni";
        case WeaponGrip::OneHanded: return "Una mano";
        case WeaponGrip::TwoHanded: return "Due mani";
        case WeaponGrip::Bow:       return "Arco";
    }
    return "?";
}

namespace {

ItemDef MakeWeapon(ItemId id, const char* key, const char* name, DiceExpr damage, float range,
                   WeaponGrip grip, const char* model) {
    ItemDef d;
    d.id = id;
    d.key = key;
    d.name = name;
    d.category = ItemCategory::Weapon;
    d.validSlot = EquipSlot::AnyHand;
    d.weapon = WeaponDef{ id, name, damage, range, grip, false, {1,1} };
    d.model = model;
    return d;
}

ItemDef MakeVersatileWeapon(ItemId id, const char* key, const char* name, DiceExpr damage, float range,
                            WeaponGrip grip, DiceExpr versDamage, const char* model) {
    ItemDef d = MakeWeapon(id, key, name, damage, range, grip, model);
    d.weapon.isVersatile = true;
    d.weapon.versatileDamage = versDamage;
    return d;
}

} // namespace

// L'ORDINE delle righe e' quello di items:: in Items.h (l'id e' l'indice).
const std::vector<ItemDef>& AllItems() {
    static const std::vector<ItemDef> table = {
        //         id                 key           name          damage    range  grip                    model
        MakeWeapon(items::Dagger,     "dagger",     "Dagger",     { 1, 4 }, 1.5f,  WeaponGrip::OneHanded,  "models/items/dagger.glb"),
        MakeWeapon(items::Shortsword, "shortsword", "Shortsword", { 1, 6 }, 1.5f,  WeaponGrip::OneHanded,  "models/items/shortsword.glb"),
        MakeVersatileWeapon(items::Longsword,  "longsword",  "Longsword",  { 1, 8 }, 1.5f,  WeaponGrip::OneHanded, { 1, 10 }, "models/items/longsword.glb"),
        MakeWeapon(items::Greatsword, "greatsword", "Greatsword", { 2, 6 }, 1.5f,  WeaponGrip::TwoHanded,  "models/items/greatsword.glb"),
        MakeWeapon(items::Greataxe,   "greataxe",   "Greataxe",   { 1, 12 }, 1.5f, WeaponGrip::TwoHanded,  "models/items/greataxe.glb"),
        MakeWeapon(items::Bow,        "bow",        "Bow",        { 1, 8 }, 8.0f,  WeaponGrip::Bow,        "models/items/bow.glb"),
        MakeWeapon(items::Longbow,    "longbow",    "Longbow",    { 1, 8 }, 12.0f, WeaponGrip::Bow,        "models/items/longbow.glb"),
    };
    return table;
}

const ItemDef* GetItem(ItemId id) {
    const std::vector<ItemDef>& t = AllItems();
    if (id < 0 || id >= static_cast<ItemId>(t.size())) return nullptr;
    return &t[static_cast<size_t>(id)];
}

ItemId FindItemId(const char* key) {
    if (!key) return kNoItem;
    for (const ItemDef& d : AllItems()) {
        if (std::strcmp(d.key, key) == 0) return d.id;
    }
    return kNoItem;
}

bool IsWeapon(ItemId id) {
    const ItemDef* d = GetItem(id);
    return d && d->category == ItemCategory::Weapon;
}

WeaponDef UnarmedStrike() {
    WeaponDef w;
    w.id = kNoItem;
    w.name = "Pugni";
    w.damage = DiceExpr{ 1, 1 };               // 1d1: danno fisso 1 (+ modificatore)
    w.range = 1.5f;
    w.grip = WeaponGrip::Unarmed;
    return w;
}

WeaponDef ResolveWeapon(ItemId id) {
    const ItemDef* d = GetItem(id);
    return (d && d->category == ItemCategory::Weapon) ? d->weapon : UnarmedStrike();
}

} // namespace core
