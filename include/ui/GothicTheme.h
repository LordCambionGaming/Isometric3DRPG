#pragma once
#include <raylib.h>
#include <iostream>

namespace ui {
namespace theme {

constexpr Color kIron       {  14,  10,  16, 255 };
constexpr Color kWood       {  38,  24,  20, 255 };
constexpr Color kFelt       {  66,  10,  22, 255 };
constexpr Color kFeltDark   {  36,   6,  14, 255 };
constexpr Color kGoldDim    { 150, 118,  56, 255 };
constexpr Color kGold       { 212, 175,  90, 255 };
constexpr Color kGoldBright { 240, 205, 110, 255 };
constexpr Color kBone       { 226, 216, 196, 255 };
constexpr Color kBlood      { 200,  30,  44, 255 };

inline Font& DiceFont() {
    static Font font{};
    static bool loaded = false;

    if (!loaded) {
        loaded = true;
        const char* path = "assets/fonts/gothic.ttf";

        if (FileExists(path)) {
            font = LoadFontEx(path, 128, nullptr, 0);
            if (font.texture.id != 0) {
                SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
                std::cout << "[Gothic] font caricato: " << path << std::endl;
                return font;
            }
        }
        std::cerr << "[Gothic] ERRORE: assets/fonts/gothic.ttf non trovato" << std::endl;
        font = GetFontDefault();
    }
    return font;
}

inline Font& UIFont() {
    static Font font = GetFontDefault();
    return font;
}

inline void DrawTextCentered(const Font& font, const char* text, float cx, float y,
                              float size, Color color, bool shadow = true) {
    const float spacing = size / 10.0f;
    const Vector2 m = MeasureTextEx(font, text, size, spacing);
    const Vector2 p = { cx - m.x / 2.0f, y };
    if (shadow) DrawTextEx(font, text, { p.x + 2.0f, p.y + 2.0f }, size, spacing, Fade(BLACK, 0.8f));
    DrawTextEx(font, text, p, size, spacing, color);
}

} // namespace theme
} // namespace ui