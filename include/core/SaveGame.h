#pragma once

#include "core/Character.h"
#include <raylib.h>

namespace core {

struct SaveGameData {
    Character character;
    Vector3 playerPosition{};
    bool hasTestMob = false;
    Vector3 testMobPosition{};
    int testMobCurrentHP = 0;
    int testMobMaxHP = 0;
};

class SaveGame {
public:
    static bool Exists();
    static bool Write(const SaveGameData& data);
    static bool Read(SaveGameData& data);
};

}
