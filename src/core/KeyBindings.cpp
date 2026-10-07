#include "core/KeyBindings.h"
#include "core/Localization.h"

#include <array>
#include <fstream>

namespace core {
namespace {

constexpr std::size_t kActionCount = static_cast<std::size_t>(KeyBindings::Action::Count);
std::array<int, kActionCount> g_keys = {
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_ENTER, KEY_ESCAPE, KEY_I
};
constexpr const char* kConfigPath = "keybindings.cfg";

bool IsValidKey(int key) {
    return key >= KEY_SPACE && key <= KEY_KB_MENU;
}

}

KeyBindings::Action KeyBindings::ActionAt(std::size_t index) {
    if (index >= ActionCount()) return Action::Count;
    return static_cast<Action>(index);
}

std::string KeyBindings::ActionName(Action action) {
    switch (action) {
        case Action::Up: return lang::Get("key.up");
        case Action::Down: return lang::Get("key.down");
        case Action::Left: return lang::Get("key.left");
        case Action::Right: return lang::Get("key.right");
        case Action::Confirm: return lang::Get("key.confirm");
        case Action::Back: return lang::Get("key.back");
        case Action::Inventory: return lang::Get("key.inventory");
        default: return {};
    }
}

int KeyBindings::Get(Action action) {
    const std::size_t index = static_cast<std::size_t>(action);
    return index < g_keys.size() ? g_keys[index] : KEY_NULL;
}

bool KeyBindings::Pressed(Action action) {
    return IsKeyPressed(Get(action));
}

bool KeyBindings::Down(Action action) {
    return IsKeyDown(Get(action));
}

std::string KeyBindings::KeyName(int key) {
    if (key >= KEY_A && key <= KEY_Z) return std::string(1, static_cast<char>('A' + key - KEY_A));
    if (key >= KEY_ZERO && key <= KEY_NINE) return std::string(1, static_cast<char>('0' + key - KEY_ZERO));
    if (key >= KEY_F1 && key <= KEY_F12) return "F" + std::to_string(key - KEY_F1 + 1);

    switch (key) {
        case KEY_UP: return lang::Get("key_name.up");
        case KEY_DOWN: return lang::Get("key_name.down");
        case KEY_LEFT: return lang::Get("key_name.left");
        case KEY_RIGHT: return lang::Get("key_name.right");
        case KEY_ENTER: return lang::Get("key_name.enter");
        case KEY_ESCAPE: return lang::Get("key_name.escape");
        case KEY_SPACE: return lang::Get("key_name.space");
        case KEY_TAB: return lang::Get("key_name.tab");
        case KEY_BACKSPACE: return lang::Get("key_name.backspace");
        case KEY_DELETE: return lang::Get("key_name.delete");
        case KEY_HOME: return lang::Get("key_name.home");
        case KEY_END: return lang::Get("key_name.end");
        case KEY_PAGE_UP: return lang::Get("key_name.page_up");
        case KEY_PAGE_DOWN: return lang::Get("key_name.page_down");
        default:
            return IsValidKey(key)
                ? TextFormat(lang::Get("key_name.other").c_str(), key)
                : lang::Get("key_name.unknown");
    }
}

void KeyBindings::Set(Action action, int key) {
    const std::size_t index = static_cast<std::size_t>(action);
    if (index >= g_keys.size() || !IsValidKey(key)) return;
    for (std::size_t other = 0; other < g_keys.size(); ++other) {
        if (other != index && g_keys[other] == key) {
            g_keys[other] = g_keys[index];
        }
    }
    g_keys[index] = key;
}

void KeyBindings::Reset() {
    g_keys = { KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_ENTER, KEY_ESCAPE, KEY_I };
}

void KeyBindings::Load() {
    std::ifstream file(kConfigPath);
    std::size_t action = 0;
    int key = KEY_NULL;
    while (file >> action >> key) {
        if (action < g_keys.size() && IsValidKey(key)) g_keys[action] = key;
    }
}

void KeyBindings::Save() {
    std::ofstream file(kConfigPath, std::ios::trunc);
    if (!file) return;
    for (std::size_t action = 0; action < g_keys.size(); ++action) {
        file << action << ' ' << g_keys[action] << '\n';
    }
}

}