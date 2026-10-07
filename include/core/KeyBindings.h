#pragma once

#include <cstddef>
#include <string>
#include <raylib.h>

namespace core {

class KeyBindings {
public:
    enum class Action {
        Up,
        Down,
        Left,
        Right,
        Confirm,
        Back,
        Inventory,
        Count
    };

    static constexpr std::size_t ActionCount() {
        return static_cast<std::size_t>(Action::Count);
    }

    static Action ActionAt(std::size_t index);
    static std::string ActionName(Action action);
    static int Get(Action action);
    static bool Pressed(Action action);
    static bool Down(Action action);
    static std::string KeyName(int key);
    static void Set(Action action, int key);
    static void Reset();
    static void Load();
    static void Save();
};

}