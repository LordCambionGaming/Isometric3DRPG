#include "core/SaveGame.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <utility>

namespace core {
namespace {

using Json = nlohmann::json;
constexpr const char* kSavePath = "saves/quick_save.json";
constexpr int kSaveVersion = 2;

Json WritePosition(Vector3 position) {
    return Json{ { "x", position.x }, { "y", position.y }, { "z", position.z } };
}

bool ReadPosition(const Json& value, Vector3& position) {
    if (!value.is_object() || !value.contains("x") || !value.contains("y") || !value.contains("z")) return false;
    position.x = value.at("x").get<float>();
    position.y = value.at("y").get<float>();
    position.z = value.at("z").get<float>();
    return std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z);
}

bool IsValidItem(ItemId id) {
    return id == kNoItem || GetItem(id) != nullptr;
}

Json WriteCharacter(const Character& character) {
    Json inventory = Json::array();
    for (const ItemStack& item : character.inventory) {
        inventory.push_back({ { "id", item.id }, { "count", item.count } });
    }

    const Equipment& equipment = character.equipment;
    return {
        { "race", static_cast<int>(character.race) },
        { "class", static_cast<int>(character.characterClass) },
        { "level", character.level },
        { "xp", character.xp },
        { "gold", character.gold },
        { "abilities", character.abilities },
        { "inventory", inventory },
        { "equipment", {
            { "head", equipment.head }, { "body", equipment.body },
            { "legs", equipment.legs }, { "feet", equipment.feet },
            { "right_hand", equipment.rightHand }, { "left_hand", equipment.leftHand },
            { "quiver_id", equipment.quiver.id }, { "quiver_count", equipment.quiver.count }
        } },
        { "vitals", {
            { "hp", character.vitals.currentHP },
            { "temp_hp", character.vitals.tempHP },
            { "mp", character.vitals.currentMP }
        } }
    };
}

bool ReadCharacter(const Json& value, Character& character) {
    if (!value.is_object()) return false;
    const int race = value.at("race").get<int>();
    const int characterClass = value.at("class").get<int>();
    const int level = value.at("level").get<int>();
    const int xp = value.at("xp").get<int>();
    if (race < 0 || race > static_cast<int>(Race::Human) ||
        characterClass < 0 || characterClass >= kCharacterClassCount || level < 1 || level > 9999 || xp < 0) return false;

    const Json& abilities = value.at("abilities");
    if (!abilities.is_array() || abilities.size() != kAbilityCount) return false;
    character.race = static_cast<Race>(race);
    character.characterClass = static_cast<CharacterClass>(characterClass);
    character.level = level;
    character.xp = xp;
    character.gold = value.value("gold", 35);
    if (character.gold < 0 || character.gold > 100000000) return false;
    for (std::size_t index = 0; index < kAbilityCount; ++index) {
        const int score = abilities.at(index).get<int>();
        if (score < 1 || score > 30) return false;
        character.abilities[index] = score;
    }

    const Json& inventory = value.at("inventory");
    if (!inventory.is_array() || inventory.size() > 1000) return false;
    character.inventory.clear();
    for (const Json& entry : inventory) {
        const int id = entry.at("id").get<int>();
        const int count = entry.at("count").get<int>();
        if (!IsValidItem(id) || id == kNoItem || count < 1 || count > 100000) return false;
        character.inventory.push_back({ id, count });
    }

    const Json& equipment = value.at("equipment");
    Equipment restoredEquipment;
    restoredEquipment.head = equipment.at("head").get<int>();
    restoredEquipment.body = equipment.at("body").get<int>();
    restoredEquipment.legs = equipment.at("legs").get<int>();
    restoredEquipment.feet = equipment.at("feet").get<int>();
    restoredEquipment.rightHand = equipment.at("right_hand").get<int>();
    restoredEquipment.leftHand = equipment.at("left_hand").get<int>();
    restoredEquipment.quiver.id = equipment.at("quiver_id").get<int>();
    restoredEquipment.quiver.count = equipment.at("quiver_count").get<int>();
    if (!IsValidItem(restoredEquipment.head) || !IsValidItem(restoredEquipment.body) ||
        !IsValidItem(restoredEquipment.legs) || !IsValidItem(restoredEquipment.feet) ||
        !IsValidItem(restoredEquipment.rightHand) || !IsValidItem(restoredEquipment.leftHand) ||
        !IsValidItem(restoredEquipment.quiver.id) || restoredEquipment.quiver.count < 0 ||
        restoredEquipment.quiver.count > 100000) return false;
    character.equipment = restoredEquipment;

    const Json& vitals = value.at("vitals");
    character.vitals.currentHP = std::clamp(vitals.at("hp").get<int>(), 0, character.MaxHP());
    character.vitals.tempHP = std::max(0, vitals.at("temp_hp").get<int>());
    character.vitals.currentMP = std::max(0, vitals.at("mp").get<int>());
    return true;
}

}

bool SaveGame::Exists() {
    return std::filesystem::exists(kSavePath);
}

bool SaveGame::Write(const SaveGameData& data) {
    try {
        std::filesystem::create_directories(std::filesystem::path(kSavePath).parent_path());
        Json root = {
            { "version", kSaveVersion },
            { "player_position", WritePosition(data.playerPosition) },
            { "character", WriteCharacter(data.character) },
            { "quests", { { "village_letter", static_cast<int>(data.quests.villageLetter) } } },
            { "test_mob", data.hasTestMob ? Json{
                { "position", WritePosition(data.testMobPosition) },
                { "hp", data.testMobCurrentHP },
                { "max_hp", data.testMobMaxHP }
            } : Json(nullptr) }
        };
        std::ofstream file(kSavePath, std::ios::trunc);
        if (!file) return false;
        file << root.dump(2);
        return file.good();
    } catch (const std::exception&) {
        return false;
    }
}

bool SaveGame::Read(SaveGameData& data) {
    try {
        std::ifstream file(kSavePath);
        if (!file) return false;
        const Json root = Json::parse(file);
        if (!root.is_object()) return false;
        const int version = root.at("version").get<int>();
        if (version < 1 || version > kSaveVersion) return false;

        SaveGameData loaded;
        if (!ReadPosition(root.at("player_position"), loaded.playerPosition) ||
            !ReadCharacter(root.at("character"), loaded.character)) return false;

        if (version >= 2 && root.contains("quests")) {
            const int questStatus = root.at("quests").at("village_letter").get<int>();
            if (questStatus < static_cast<int>(QuestStatus::NotStarted) ||
                questStatus > static_cast<int>(QuestStatus::Completed)) return false;
            loaded.quests.villageLetter = static_cast<QuestStatus>(questStatus);
        }

        const Json& mob = root.at("test_mob");
        if (!mob.is_null()) {
            if (!ReadPosition(mob.at("position"), loaded.testMobPosition)) return false;
            loaded.testMobMaxHP = mob.at("max_hp").get<int>();
            loaded.testMobCurrentHP = mob.at("hp").get<int>();
            if (loaded.testMobMaxHP < 1 || loaded.testMobMaxHP > 100000 ||
                loaded.testMobCurrentHP < 0 || loaded.testMobCurrentHP > loaded.testMobMaxHP) return false;
            loaded.hasTestMob = true;
        }
        data = std::move(loaded);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

}
