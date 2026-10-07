#pragma once
#include <vector>
#include "core/Dice.h"

// Catalogo degli oggetti del gioco: UNA tabella, fuori da Character e da Mob.
// Character e Mob conservano solo ItemId (e quantita'); nome, dadi, portata, impugnatura, modello 3D e icona
// si leggono sempre da qui. Aggiungere un oggetto = una riga in Items.cpp (e una voce in items:: qui sotto).

namespace core {

using ItemId = int;
constexpr ItemId kNoItem = -1;

enum class ItemCategory { Weapon, Armor, Consumable, Misc };

enum class EquipSlot { None, Head, Body, Legs, Feet, RightHand, LeftHand, Quiver, AnyHand };


// Come si impugna un'arma: decide l'animazione d'attacco.
//   Unarmed   -> Attack      (pugni, nessuna arma)
//   OneHanded -> Attack1H    (mano destra)
//   TwoHanded -> Attack2H    (due mani sull'impugnatura)
//   Bow       -> Bow         (arco nella mano sinistra, corda nella destra)
enum class WeaponGrip { Unarmed, OneHanded, TwoHanded, Bow };

const char* ToString(ItemCategory category);
const char* ToString(WeaponGrip grip);          // "Pugni", "Una mano", "Due mani", "Arco"

// Un'arma "risolta": quello che serve in combattimento. Si ottiene con ResolveWeapon(id) / Character::ActiveWeapon().
struct WeaponDef {
    ItemId id = kNoItem;                         // kNoItem per i pugni
    const char* name = "";
    DiceExpr damage{ 1, 8 };                     // Default: 1d8
    float range = 1.5f;                          // Portata in metri (1.5m = mischia, 8m = distanza)
    WeaponGrip grip = WeaponGrip::OneHanded;
    bool isVersatile = false;
    DiceExpr versatileDamage{ 1, 10 };
};

struct ItemDef {
    ItemId id = kNoItem;                         // = indice nella tabella (vedi items::)
    const char* key = "";                        // chiave stabile, per salvataggi e script ("longsword")
    const char* name = "";                       // nome mostrato
    ItemCategory category = ItemCategory::Misc;
    EquipSlot validSlot = EquipSlot::None;
    WeaponDef weapon{};                          // valido solo se category == Weapon
    const char* model = "";                      // asset .glb sotto assets/ ("" finche' non esiste)
    const char* icon = "";                       // immagine dell'inventario ("" finche' non esiste)
};

// Una riga della lista oggetti di chi li possiede (Character, Mob, bauli...).
struct ItemStack {
    ItemId id = kNoItem;
    int count = 1;
};

// Id degli oggetti: l'ordine DEVE coincidere con la tabella in Items.cpp.
namespace items {
enum : ItemId { Dagger, Shortsword, Longsword, Greatsword, Greataxe, Bow, Longbow, Arrow, Quiver, Count };
}

const std::vector<ItemDef>& AllItems();
const ItemDef* GetItem(ItemId id);               // nullptr se l'id non esiste
ItemId FindItemId(const char* key);              // kNoItem se la chiave non esiste
bool IsWeapon(ItemId id);

WeaponDef UnarmedStrike();                       // pugni: 1 di danno fisso, mischia
WeaponDef ResolveWeapon(ItemId id);              // pugni se id non e' un'arma

} // namespace core
