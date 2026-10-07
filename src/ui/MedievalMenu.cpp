#include "ui/MedievalMenu.h"

#include "ui/GothicTheme.h"
#include <algorithm>

namespace ui::menu {
namespace {

float RowHeight(int count) {
    return count > 7 ? 52.0f : 64.0f;
}

Rectangle PanelBounds(int count) {
    const float width = std::min(620.0f, static_cast<float>(GetScreenWidth()) - 40.0f);
    const float height = 192.0f + RowHeight(count) * count;
    return { (GetScreenWidth() - width) * 0.5f, (GetScreenHeight() - height) * 0.5f, width, height };
}

}

Rectangle OptionBounds(int index, int count) {
    const Rectangle panel = PanelBounds(count);
    const float rowHeight = RowHeight(count);
    return { panel.x + 28.0f, panel.y + 138.0f + rowHeight * index,
             panel.width - 56.0f, rowHeight };
}

Rectangle SliderBounds(int index, int count) {
    const Rectangle row = OptionBounds(index, count);
    return { row.x + row.width - 148.0f, row.y + row.height * 0.5f - 2.0f, 92.0f, 4.0f };
}

int OptionAt(Vector2 point, int count) {
    for (int index = 0; index < count; ++index) {
        if (CheckCollisionPointRec(point, OptionBounds(index, count))) return index;
    }
    return -1;
}

void Draw(const char* title, const char* subtitle, const std::vector<MenuItem>& items, int selected,
          bool useTitleFont) {
    const int count = static_cast<int>(items.size());
    const Rectangle panel = PanelBounds(count);
    const Font& font = theme::UIFont();
    const Font& headingFont = useTitleFont ? theme::TitleFont() : font;

    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(),
                           Color{ 8, 9, 11, 255 }, Color{ 27, 16, 17, 255 });
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{ 0, 0, 0, 70 });

    Rectangle shadow = { panel.x + 7.0f, panel.y + 8.0f, panel.width, panel.height };
    DrawRectangleRec(shadow, Color{ 0, 0, 0, 150 });
    DrawRectangleRec(panel, Color{ 18, 15, 15, 248 });
    DrawRectangleLinesEx(panel, 2.0f, theme::kGoldDim);
    Rectangle inner = { panel.x + 8.0f, panel.y + 8.0f, panel.width - 16.0f, panel.height - 16.0f };
    DrawRectangleLinesEx(inner, 1.0f, Fade(theme::kGold, 0.42f));

    const float centerX = panel.x + panel.width * 0.5f;
    DrawPoly({ centerX, panel.y + 26.0f }, 4, 7.0f, 45.0f, theme::kGoldBright);
    DrawLineEx({ centerX - 68.0f, panel.y + 26.0f }, { centerX - 14.0f, panel.y + 26.0f }, 1.0f, theme::kGoldDim);
    DrawLineEx({ centerX + 14.0f, panel.y + 26.0f }, { centerX + 68.0f, panel.y + 26.0f }, 1.0f, theme::kGoldDim);
    float titleSize = useTitleFont ? 48.0f : 38.0f;
    while (titleSize > 30.0f && MeasureTextEx(headingFont, title, titleSize, titleSize / 10.0f).x > panel.width - 64.0f)
        titleSize -= 2.0f;
    theme::DrawTextCentered(headingFont, title, centerX, panel.y + (useTitleFont ? 32.0f : 36.0f),
                            titleSize, theme::kGoldBright);
    float subtitleSize = 28.0f;
    while (subtitleSize > 20.0f && MeasureTextEx(font, subtitle, subtitleSize, subtitleSize / 10.0f).x > panel.width - 48.0f)
        subtitleSize -= 2.0f;
    theme::DrawTextCentered(font, subtitle, centerX, panel.y + 82.0f, subtitleSize, theme::kGoldBright);
    DrawLineEx({ panel.x + 30.0f, panel.y + 121.0f },
               { panel.x + panel.width - 30.0f, panel.y + 121.0f }, 1.0f, Fade(theme::kGoldDim, 0.8f));

    for (int index = 0; index < count; ++index) {
        const Rectangle row = OptionBounds(index, count);
        const bool active = index == selected;
        if (active) {
            DrawRectangleRec(row, Color{ 100, 66, 25, 65 });
            DrawRectangleLinesEx(row, 1.0f, Fade(theme::kGold, 0.72f));
            DrawPoly({ row.x + 16.0f, row.y + row.height * 0.5f }, 4, 4.0f, 45.0f, theme::kGoldBright);
        }

        const float textY = row.y + (row.height - 33.0f) * 0.5f;
        DrawTextEx(font, items[index].label.c_str(), { row.x + 36.0f, textY }, 30.0f, 1.0f,
                   active ? theme::kGoldBright : theme::kBone);
        if (items[index].sliderValue >= 0.0f) {
            const Rectangle slider = SliderBounds(index, count);
            DrawRectangleRec(slider, Color{ 55, 48, 38, 255 });
            DrawRectangleRec({ slider.x, slider.y, slider.width * items[index].sliderValue, slider.height },
                             theme::kGold);
            DrawCircleV({ slider.x + slider.width * items[index].sliderValue, slider.y + slider.height * 0.5f },
                        6.0f, active ? theme::kGoldBright : theme::kBone);
            DrawTextEx(font, items[index].detail.c_str(),
                       { slider.x + slider.width + 12.0f, textY + 2.0f },
                       26.0f, 1.0f, active ? theme::kGold : theme::kGoldDim);
        } else if (items[index].hasStepper) {
            const float arrowsY = textY + 1.0f;
            DrawTextEx(font, "<", { row.x + row.width - 162.0f, arrowsY }, 26.0f, 1.0f,
                       active ? theme::kGoldBright : theme::kGoldDim);
            const Vector2 detailSize = MeasureTextEx(font, items[index].detail.c_str(), 26.0f, 1.0f);
            DrawTextEx(font, items[index].detail.c_str(),
                       { row.x + row.width - 112.0f - detailSize.x * 0.5f, textY + 2.0f },
                       26.0f, 1.0f, active ? theme::kGold : theme::kGoldDim);
            DrawTextEx(font, ">", { row.x + row.width - 40.0f, arrowsY }, 26.0f, 1.0f,
                       active ? theme::kGoldBright : theme::kGoldDim);
        } else if (!items[index].detail.empty()) {
            const Vector2 detailSize = MeasureTextEx(font, items[index].detail.c_str(), 26.0f, 1.0f);
            DrawTextEx(font, items[index].detail.c_str(),
                       { row.x + row.width - detailSize.x - 16.0f, textY + 2.0f },
                       26.0f, 1.0f, active ? theme::kGold : theme::kGoldDim);
        }
    }

    const float ornamentY = panel.y + panel.height - 21.0f;
    DrawLineEx({ centerX - 48.0f, ornamentY }, { centerX - 8.0f, ornamentY }, 1.0f, theme::kGoldDim);
    DrawPoly({ centerX, ornamentY }, 4, 4.0f, 45.0f, theme::kGold);
    DrawLineEx({ centerX + 8.0f, ornamentY }, { centerX + 48.0f, ornamentY }, 1.0f, theme::kGoldDim);
}

}