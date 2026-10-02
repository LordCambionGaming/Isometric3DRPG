#pragma once
#include <raylib.h>
#include <iostream>

namespace ui {
namespace theme {

constexpr Color kIron      {  14,  10,  16, 255 };
constexpr Color kWood      {  38,  24,  20, 255 };
constexpr Color kFelt      {  66,  10,  22, 255 };
constexpr Color kFeltDark  {  36,   6,  14, 255 };
constexpr Color kGoldDim   { 150, 118,  56, 255 };
constexpr Color kGold      { 212, 175,  90, 255 };
constexpr Color kGoldBright{ 240, 205, 110, 255 };
constexpr Color kBone      { 226, 216, 196, 255 };
constexpr Color kBlood     { 200,  30,  44, 255 };

// Font dei DADI: gothic.ttf (Old English) in assets/fonts/. Usato SOLO per i numeri sulle facce.
// Deve essere chiamata dopo InitWindow. Cerca in piu' percorsi relativi
// perche' con Visual Studio + CMake la cartella di lavoro e' quella della build.
inline Font& DiceFont() {
    static Font font{};
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        // il font vive in src/assets/fonts/ (e viene copiato in <build>/assets/fonts/ da CMake):
        // si prova sia dalla cartella di lavoro sia da quella dell'eseguibile
        const char* dirs[] = {
            "assets/fonts/",
            "src/assets/fonts/",
            "../assets/fonts/",
            "../src/assets/fonts/",
            "../../assets/fonts/",
            "../../src/assets/fonts/",
            "../../../assets/fonts/",
            "../../../src/assets/fonts/",
            "../../../../assets/fonts/",
            "../../../../src/assets/fonts/"
        };
        const char* names[] = { "gothic.ttf", "OldeEnglish.ttf" };
        const char* bases[] = { "", GetApplicationDirectory() };
        for (const char* base : bases) {
            for (const char* d : dirs) {
                for (const char* n : names) {
                    const char* path = TextFormat("%s%s%s", base, d, n);
                    if (FileExists(path)) {
                        font = LoadFontEx(path, 128, nullptr, 0);
                        if (font.texture.id != 0) {
                            SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
                            std::cout << "[Gothic] font caricato: " << path << std::endl;
                            return font;
                        }
                    }
                }
            }
        }
        std::cerr << "[Gothic] gothic.ttf (font dei dadi) NON trovato: uso il font di default" << std::endl;
        font = GetFontDefault();
    }
    return font;
}

// Font di tutta l'interfaccia (menu, banner, HUD...). Per ora e' quello di default di raylib:
// quando avrai il font definitivo basta caricarlo qui, in un solo punto.
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