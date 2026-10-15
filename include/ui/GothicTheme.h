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
    static Font font{};
    static bool loaded = false;

    if (!loaded) {
        loaded = true;
        const char* path = "assets/fonts/texts.ttf";
        if (FileExists(path)) {
            font = LoadFontEx(path, 128, nullptr, 0);
            if (font.texture.id != 0) {
                SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
                std::cout << "[UI] font caricato: " << path << std::endl;
                return font;
            }
        }
        std::cerr << "[UI] ERRORE: assets/fonts/texts.ttf non trovato" << std::endl;
        font = GetFontDefault();
    }
    return font;
}

inline Font& TitleFont() {
    static Font font{};
    static bool loaded = false;

    if (!loaded) {
        loaded = true;
        const char* path = "assets/fonts/title.ttf";
        if (FileExists(path)) {
            font = LoadFontEx(path, 160, nullptr, 0);
            if (font.texture.id != 0) {
                SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
                return font;
            }
        }
        std::cerr << "[UI] ERRORE: assets/fonts/title.ttf non trovato" << std::endl;
        font = UIFont();
    }
    return font;
}

inline void UnloadFonts() {
    Font& df = DiceFont();
    if (df.texture.id != 0 && df.texture.id != GetFontDefault().texture.id) {
        UnloadFont(df);
        df = GetFontDefault();
    }
    Font& uf = UIFont();
    if (uf.texture.id != 0 && uf.texture.id != GetFontDefault().texture.id) {
        UnloadFont(uf);
        uf = GetFontDefault();
    }
    Font& tf = TitleFont();
    if (tf.texture.id != 0 && tf.texture.id != GetFontDefault().texture.id && tf.texture.id != uf.texture.id) {
        UnloadFont(tf);
        tf = GetFontDefault();
    }
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