#include "states/SettingsState.h"

#include "audio/AudioManager.h"
#include "core/Game.h"
#include "core/KeyBindings.h"
#include "core/Localization.h"
#include "rendering/Renderer.h"
#include "states/MainMenuState.h"
#include "states/PauseState.h"
#include "ui/MedievalMenu.h"
#include <raylib.h>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace {

struct Resolution {
    int width;
    int height;
};

constexpr std::array<Resolution, 4> kResolutions = {{
    { 1024, 768 }, { 1280, 720 }, { 1600, 900 }, { 1920, 1080 }
}};

std::string VolumeLabel(float volume) {
    return std::to_string(static_cast<int>(std::lround(volume * 100.0f))) + "%";
}

}

SettingsState::SettingsState(Game* game, PreviousState prev)
    : GameState(game), m_prev(prev) {
    m_type = GameStateType::Loading;
}

void SettingsState::Enter() {}
void SettingsState::Exit() {}

void SettingsState::ReturnToPreviousState() {
    if (!m_game) return;
    if (m_prev == FromPause) m_game->SetState(std::make_unique<PauseState>(m_game));
    else m_game->SetState(std::make_unique<MainMenuState>(m_game));
}

void SettingsState::Update(float deltaTime) {
    (void)deltaTime;
}

void SettingsState::Render() {
    std::vector<ui::menu::MenuItem> items;
    if (m_page == Page::General) {
        const Renderer* renderer = m_game ? m_game->GetRenderer() : nullptr;
        const AudioManager* audio = m_game ? m_game->GetAudioManager() : nullptr;
        items = {
                        { lang::Get("settings.language"), lang::Get(lang::CurrentLanguage() == lang::Language::Italian ? "settings.italian" : "settings.english") },
                        { lang::Get("settings.display_mode"), lang::Get(renderer && renderer->IsFullscreen() ? "settings.fullscreen" : "settings.windowed") },
                        { lang::Get("settings.resolution"), renderer ? std::to_string(renderer->GetWindowedWidth()) + " x " +
                            std::to_string(renderer->GetWindowedHeight()) : "--", -1.0f, true },
                        { lang::Get("settings.vsync"), lang::Get(renderer && renderer->IsVSyncEnabled() ? "settings.enabled" : "settings.disabled") },
                        { lang::Get("settings.master_volume"), audio ? VolumeLabel(audio->GetMasterVolume()) : "--", audio ? audio->GetMasterVolume() : -1.0f },
                        { lang::Get("settings.music_volume"), audio ? VolumeLabel(audio->GetMusicVolume()) : "--", audio ? audio->GetMusicVolume() : -1.0f },
                        { lang::Get("settings.effects_volume"), audio ? VolumeLabel(audio->GetSoundVolume()) : "--", audio ? audio->GetSoundVolume() : -1.0f },
                        { lang::Get("settings.controls"), lang::Get("settings.configure") },
                        { lang::Get("settings.reset"), "" },
                        { lang::Get("settings.back"), "" }
        };
                ui::menu::Draw(lang::Get("settings.title").c_str(), lang::Get("settings.subtitle").c_str(), items, m_selectedOption);
        return;
    }

    items.reserve(core::KeyBindings::ActionCount() + 2);
    for (std::size_t index = 0; index < core::KeyBindings::ActionCount(); ++index) {
        const auto action = core::KeyBindings::ActionAt(index);
        items.push_back({ core::KeyBindings::ActionName(action), core::KeyBindings::KeyName(core::KeyBindings::Get(action)) });
    }
    items.push_back({ lang::Get("settings.reset"), "" });
    items.push_back({ lang::Get("settings.back"), "" });
    const std::string controlsSubtitle = lang::Get(m_waitingForKey ? "settings.press_key" : "settings.controls_subtitle");
    ui::menu::Draw(lang::Get("settings.controls").c_str(), controlsSubtitle.c_str(), items, m_selectedOption);
}

void SettingsState::ApplyGeneralSetting(int direction) {
    Renderer* renderer = m_game ? m_game->GetRenderer() : nullptr;
    AudioManager* audio = m_game ? m_game->GetAudioManager() : nullptr;
    switch (m_selectedOption) {
        case 0:
            lang::SetLanguage(lang::CurrentLanguage() == lang::Language::Italian
                ? lang::Language::English : lang::Language::Italian);
            break;
        case 1:
            if (renderer) renderer->SetFullscreen(!renderer->IsFullscreen());
            break;
        case 2:
            if (renderer) {
                int current = -1;
                for (int index = 0; index < static_cast<int>(kResolutions.size()); ++index) {
                    if (kResolutions[index].width == renderer->GetWindowedWidth() &&
                        kResolutions[index].height == renderer->GetWindowedHeight()) current = index;
                }
                const int next = (current + direction + static_cast<int>(kResolutions.size())) %
                                 static_cast<int>(kResolutions.size());
                renderer->SetWindowedResolution(kResolutions[next].width, kResolutions[next].height);
            }
            break;
        case 3:
            if (renderer) renderer->SetVSyncEnabled(!renderer->IsVSyncEnabled());
            break;
        case 4:
            if (audio) audio->SetMasterVolume(audio->GetMasterVolume() + direction * 0.05f);
            break;
        case 5:
            if (audio) audio->SetMusicVolume(audio->GetMusicVolume() + direction * 0.05f);
            break;
        case 6:
            if (audio) audio->SetSoundVolume(audio->GetSoundVolume() + direction * 0.05f);
            break;
    }
}

void SettingsState::HandleInput() {
    if (m_page == Page::General) {
        constexpr int itemCount = 10;
        const Vector2 mouse = GetMousePosition();
        const int clickedOption = IsMouseButtonPressed(MOUSE_LEFT_BUTTON)
            ? ui::menu::OptionAt(mouse, itemCount) : -1;
        const bool clicked = clickedOption >= 0;
        if (clicked) m_selectedOption = clickedOption;

        if (m_draggingSlider >= 0) {
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                const Rectangle slider = ui::menu::SliderBounds(m_draggingSlider, itemCount);
                const float normalized = Clamp((mouse.x - slider.x) / slider.width, 0.0f, 1.0f);
                const float value = std::round(normalized * 20.0f) / 20.0f;
                AudioManager* audio = m_game ? m_game->GetAudioManager() : nullptr;
                if (audio) {
                    if (m_draggingSlider == 4) audio->SetMasterVolume(value);
                    else if (m_draggingSlider == 5) audio->SetMusicVolume(value);
                    else audio->SetSoundVolume(value);
                }
            } else {
                m_draggingSlider = -1;
            }
            return;
        }

        if (clicked && m_selectedOption == 2) {
            const Rectangle row = ui::menu::OptionBounds(m_selectedOption, itemCount);
            const int direction = mouse.x < row.x + row.width - 88.0f ? -1 : 1;
            ApplyGeneralSetting(direction);
            return;
        }
        if (clicked && m_selectedOption >= 4 && m_selectedOption <= 6) {
            const Rectangle slider = ui::menu::SliderBounds(m_selectedOption, itemCount);
            const Rectangle hitArea = { slider.x - 6.0f, ui::menu::OptionBounds(m_selectedOption, itemCount).y,
                                        slider.width + 12.0f, ui::menu::OptionBounds(m_selectedOption, itemCount).height };
            if (CheckCollisionPointRec(mouse, hitArea)) {
                m_draggingSlider = m_selectedOption;
                const float normalized = Clamp((mouse.x - slider.x) / slider.width, 0.0f, 1.0f);
                const float value = std::round(normalized * 20.0f) / 20.0f;
                if (AudioManager* audio = m_game ? m_game->GetAudioManager() : nullptr) {
                    if (m_selectedOption == 4) audio->SetMasterVolume(value);
                    else if (m_selectedOption == 5) audio->SetMusicVolume(value);
                    else audio->SetSoundVolume(value);
                }
                return;
            }
            ApplyGeneralSetting(mouse.x < slider.x ? -1 : 1);
            return;
        }

        if (core::KeyBindings::Pressed(core::KeyBindings::Action::Up)) {
            m_selectedOption = (m_selectedOption + itemCount - 1) % itemCount;
            return;
        }
        if (core::KeyBindings::Pressed(core::KeyBindings::Action::Down)) {
            m_selectedOption = (m_selectedOption + 1) % itemCount;
            return;
        }
        if (core::KeyBindings::Pressed(core::KeyBindings::Action::Back)) {
            ReturnToPreviousState();
            return;
        }
        if (core::KeyBindings::Pressed(core::KeyBindings::Action::Left)) {
            ApplyGeneralSetting(-1);
            return;
        }
        if (core::KeyBindings::Pressed(core::KeyBindings::Action::Right)) {
            ApplyGeneralSetting(1);
            return;
        }
        if (!clicked && !core::KeyBindings::Pressed(core::KeyBindings::Action::Confirm)) return;

        if (m_selectedOption <= 6) {
            ApplyGeneralSetting(1);
        } else if (m_selectedOption == 7) {
            m_page = Page::Controls;
            m_selectedOption = 0;
        } else if (m_selectedOption == 8) {
            lang::SetLanguage(lang::Language::Italian);
            if (Renderer* renderer = m_game ? m_game->GetRenderer() : nullptr) {
                renderer->SetFullscreen(true);
                renderer->SetVSyncEnabled(true);
                renderer->SetWindowedResolution(1280, 720);
            }
            if (AudioManager* audio = m_game ? m_game->GetAudioManager() : nullptr) {
                audio->SetMasterVolume(1.0f);
                audio->SetMusicVolume(1.0f);
                audio->SetSoundVolume(1.0f);
            }
        } else {
            ReturnToPreviousState();
        }
        return;
    }

    if (m_waitingForKey) {
        const int key = GetKeyPressed();
        if (key == KEY_ESCAPE) {
            m_waitingForKey = false;
        } else if (key != KEY_NULL) {
            const auto action = core::KeyBindings::ActionAt(static_cast<std::size_t>(m_selectedOption));
            core::KeyBindings::Set(action, key);
            core::KeyBindings::Save();
            m_waitingForKey = false;
        }
        return;
    }

    const int itemCount = static_cast<int>(core::KeyBindings::ActionCount()) + 2;
    const int clickedOption = IsMouseButtonPressed(MOUSE_LEFT_BUTTON)
        ? ui::menu::OptionAt(GetMousePosition(), itemCount) : -1;
    const bool clicked = clickedOption >= 0;
    if (clicked) m_selectedOption = clickedOption;
    if (core::KeyBindings::Pressed(core::KeyBindings::Action::Up)) {
        m_selectedOption = (m_selectedOption + itemCount - 1) % itemCount;
        return;
    }
    if (core::KeyBindings::Pressed(core::KeyBindings::Action::Down)) {
        m_selectedOption = (m_selectedOption + 1) % itemCount;
        return;
    }
    if (core::KeyBindings::Pressed(core::KeyBindings::Action::Back)) {
        m_page = Page::General;
        m_selectedOption = 7;
        return;
    }
    if (!clicked && !core::KeyBindings::Pressed(core::KeyBindings::Action::Confirm)) return;

    if (m_selectedOption < static_cast<int>(core::KeyBindings::ActionCount())) {
        while (GetKeyPressed() != KEY_NULL) {}
        m_waitingForKey = true;
    } else if (m_selectedOption == static_cast<int>(core::KeyBindings::ActionCount())) {
        core::KeyBindings::Reset();
        core::KeyBindings::Save();
    } else {
        m_page = Page::General;
        m_selectedOption = 7;
    }
}
