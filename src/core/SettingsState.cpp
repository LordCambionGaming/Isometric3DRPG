#include "core/SettingsState.h"
#include <memory>
#include "core/Game.h"
#include <raylib.h>
#include <iostream>
#include <core/PauseState.h>
#include <core/MainMenuState.h>

SettingsState::SettingsState(class Game* game, PreviousState prev)
    : GameState(game), m_prev(prev) { m_type = GameStateType::Loading; }

void SettingsState::Enter() {}
void SettingsState::Exit() {}

void SettingsState::Update(float dt) {
    // Volume settings con LEFT/RIGHT
    static float masterVol = 0.8f, musicVol = 0.6f, sfxVol = 0.7f;
    static int selected = 0;
    if (IsKeyPressed(KEY_DOWN)) selected = (selected + 1) % 4;
    if (IsKeyPressed(KEY_UP)) selected = (selected + 3) % 4;
    if (IsKeyPressed(KEY_LEFT)) {
        if (selected == 0) masterVol = std::max(0.0f, masterVol - 0.1f);
        else if (selected == 1) musicVol = std::max(0.0f, musicVol - 0.1f);
        else if (selected == 2) sfxVol = std::max(0.0f, sfxVol - 0.1f);
    }
    if (IsKeyPressed(KEY_RIGHT)) {
        if (selected == 0) masterVol = std::min(1.0f, masterVol + 0.1f);
        else if (selected == 1) musicVol = std::min(1.0f, musicVol + 0.1f);
        else if (selected == 2) sfxVol = std::min(1.0f, sfxVol + 0.1f);
    }
}

void SettingsState::Render() {
    DrawText("SETTINGS", 350, 80, 30, WHITE);
    static float masterVol = 0.8f, musicVol = 0.6f, sfxVol = 0.7f;
    char buf[64];
    snprintf(buf, sizeof(buf), "Master Volume: %.1f%%", masterVol * 100.0f);
    DrawText(buf, 330, 150, 20, LIGHTGRAY);
    snprintf(buf, sizeof(buf), "Music Volume: %.1f%%", musicVol * 100.0f);
    DrawText(buf, 330, 180, 20, LIGHTGRAY);
    snprintf(buf, sizeof(buf), "SFX Volume: %.1f%%", sfxVol * 100.0f);
    DrawText(buf, 330, 210, 20, LIGHTGRAY);
    DrawText("LEFT/RIGHT: modificare  ESC: indietro", 330, 260, 16, DARKGRAY);
}

void SettingsState::HandleInput() {
    if (IsKeyPressed(KEY_ESCAPE)) {
        std::cout << "SETTINGS ESC -> previous=" << (m_prev == FromPause ? "Pause" : "MainMenu") << std::endl;
        if (m_prev == FromPause) {
            if (m_game) m_game->SetState(std::make_unique<PauseState>(m_game));
        } else {
            if (m_game) m_game->SetState(std::make_unique<MainMenuState>(m_game));
        }
    }
}
