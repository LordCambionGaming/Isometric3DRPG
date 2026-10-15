#include "ui/MedievalMenu.h"
#include "ui/GothicTheme.h"
#include <algorithm>
#include <sstream>
#include <vector>

namespace ui::menu {
namespace {

float RowHeight(int count) {
    return count > 7 ? 48.0f : 58.0f;
}

std::vector<std::string> WrapText(const Font& font, const std::string& text, float maxWidth, float fontSize) {
    std::vector<std::string> lines;
    if (text.empty()) return lines;
    
    std::istringstream words(text);
    std::string word;
    std::string currentLine;
    const float spacing = fontSize / 10.0f;

    while (words >> word) {
        std::string testLine = currentLine.empty() ? word : currentLine + " " + word;
        if (MeasureTextEx(font, testLine.c_str(), fontSize, spacing).x <= maxWidth) {
            currentLine = testLine;
        } else {
            if (!currentLine.empty()) lines.push_back(currentLine);
            currentLine = word;
        }
    }
    if (!currentLine.empty()) lines.push_back(currentLine);
    return lines;
}

float CalculateSubtitleHeight(const Font& font, const char* subtitle, float maxWidth, float& outFontSize, std::vector<std::string>& outLines) {
    if (!subtitle || subtitle[0] == '\0') {
        outFontSize = 24.0f;
        outLines.clear();
        return 0.0f;
    }

    outFontSize = 24.0f;
    outLines = WrapText(font, subtitle, maxWidth, outFontSize);
    if (outLines.size() > 3) {
        outFontSize = 20.0f;
        outLines = WrapText(font, subtitle, maxWidth, outFontSize);
    }

    const float lineHeight = outFontSize * 1.25f;
    return static_cast<float>(outLines.size()) * lineHeight;
}

float GetExtraHeaderHeight(const char* subtitle) {
    if (!subtitle || subtitle[0] == '\0') return 0.0f;
    const float screenW = static_cast<float>(GetScreenWidth());
    const float maxTextWidth = std::clamp(screenW * 0.58f, 580.0f, std::min(860.0f, screenW - 40.0f)) - 56.0f;
    float fontSize = 24.0f;
    std::vector<std::string> lines;
    const float subH = CalculateSubtitleHeight(theme::UIFont(), subtitle, maxTextWidth, fontSize, lines);
    return std::max(0.0f, subH - 30.0f);
}

Rectangle PanelBounds(int count, float extraHeaderHeight = 0.0f) {
    const float screenW = static_cast<float>(GetScreenWidth());
    const float screenH = static_cast<float>(GetScreenHeight());
    const float width = std::clamp(screenW * 0.58f, 580.0f, std::min(860.0f, screenW - 40.0f));
    const float baseHeader = 150.0f + extraHeaderHeight;
    const float height = baseHeader + RowHeight(count) * count + 40.0f;
    return { (screenW - width) * 0.5f, std::max(10.0f, (screenH - height) * 0.5f), width, height };
}

float GetStartRowY(int count, const char* subtitle) {
    const float screenW = static_cast<float>(GetScreenWidth());
    const float maxTextWidth = std::clamp(screenW * 0.58f, 580.0f, std::min(860.0f, screenW - 40.0f)) - 56.0f;
    float subtitleFontSize = 24.0f;
    std::vector<std::string> subtitleLines;
    const float subtitleTotalH = CalculateSubtitleHeight(theme::UIFont(), subtitle, maxTextWidth, subtitleFontSize, subtitleLines);
    const float extraHeader = std::max(0.0f, subtitleTotalH - 30.0f);
    const Rectangle panel = PanelBounds(count, extraHeader);

    const float titleSize = 36.0f;
    float currentSubY = panel.y + 32.0f + titleSize + 8.0f;
    const float subLineStep = subtitleFontSize * 1.25f;
    currentSubY += static_cast<float>(subtitleLines.size()) * subLineStep;
    const float dividerY = currentSubY + 8.0f;
    return dividerY + 12.0f;
}

} // namespace

Rectangle OptionBounds(int index, int count, const char* subtitle) {
    const float extraHeader = GetExtraHeaderHeight(subtitle);
    const Rectangle panel = PanelBounds(count, extraHeader);
    const float startRowY = GetStartRowY(count, subtitle);
    const float rowHeight = RowHeight(count);
    return { panel.x + 28.0f, startRowY + index * rowHeight, panel.width - 56.0f, rowHeight };
}

Rectangle SliderBounds(int index, int count, const char* subtitle) {
    const Rectangle row = OptionBounds(index, count, subtitle);
    return { row.x + row.width - 180.0f, row.y + row.height * 0.5f - 2.0f, 100.0f, 4.0f };
}

Rectangle StepperLeftArrowBounds(int index, int count, const char* subtitle) {
    const Rectangle row = OptionBounds(index, count, subtitle);
    return { row.x + row.width - 240.0f, row.y, 40.0f, row.height };
}

Rectangle StepperRightArrowBounds(int index, int count, const char* subtitle) {
    const Rectangle row = OptionBounds(index, count, subtitle);
    return { row.x + row.width - 40.0f, row.y, 40.0f, row.height };
}

int OptionAt(Vector2 point, int count, const char* subtitle) {
    for (int index = 0; index < count; ++index) {
        if (CheckCollisionPointRec(point, OptionBounds(index, count, subtitle))) return index;
    }
    return -1;
}

void Draw(const char* title, const char* subtitle, const std::vector<MenuItem>& items, int selected,
          bool useTitleFont) {
    const int count = static_cast<int>(items.size());
    const Font& font = theme::UIFont();
    const Font& headingFont = useTitleFont ? theme::TitleFont() : font;

    const float screenW = static_cast<float>(GetScreenWidth());
    const float screenH = static_cast<float>(GetScreenHeight());

    float subtitleFontSize = 24.0f;
    std::vector<std::string> subtitleLines;
    const float maxTextWidth = std::clamp(screenW * 0.58f, 580.0f, std::min(860.0f, screenW - 40.0f)) - 56.0f;
    const float subtitleTotalH = CalculateSubtitleHeight(font, subtitle, maxTextWidth, subtitleFontSize, subtitleLines);
    const float extraHeader = std::max(0.0f, subtitleTotalH - 30.0f);

    const Rectangle panel = PanelBounds(count, extraHeader);

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
    DrawPoly({ centerX, panel.y + 24.0f }, 4, 7.0f, 45.0f, theme::kGoldBright);
    DrawLineEx({ centerX - 68.0f, panel.y + 24.0f }, { centerX - 14.0f, panel.y + 24.0f }, 1.0f, theme::kGoldDim);
    DrawLineEx({ centerX + 14.0f, panel.y + 24.0f }, { centerX + 68.0f, panel.y + 24.0f }, 1.0f, theme::kGoldDim);

    float titleSize = useTitleFont ? 44.0f : 36.0f;
    while (titleSize > 26.0f && MeasureTextEx(headingFont, title, titleSize, titleSize / 10.0f).x > panel.width - 64.0f)
        titleSize -= 2.0f;
    theme::DrawTextCentered(headingFont, title, centerX, panel.y + 32.0f, titleSize, theme::kGoldBright);

    float currentSubY = panel.y + 32.0f + titleSize + 8.0f;
    const float subLineStep = subtitleFontSize * 1.25f;
    for (const auto& line : subtitleLines) {
        theme::DrawTextCentered(font, line.c_str(), centerX, currentSubY, subtitleFontSize, theme::kBone);
        currentSubY += subLineStep;
    }

    const float dividerY = currentSubY + 8.0f;
    DrawLineEx({ panel.x + 30.0f, dividerY },
               { panel.x + panel.width - 30.0f, dividerY }, 1.0f, Fade(theme::kGoldDim, 0.8f));

    const float rowHeight = RowHeight(count);
    const float startRowY = dividerY + 12.0f;

    for (int index = 0; index < count; ++index) {
        const Rectangle row = { panel.x + 28.0f, startRowY + index * rowHeight, panel.width - 56.0f, rowHeight };
        const bool active = index == selected;
        if (active) {
            DrawRectangleRec(row, Color{ 100, 66, 25, 65 });
            DrawRectangleLinesEx(row, 1.0f, Fade(theme::kGold, 0.72f));
            DrawPoly({ row.x + 16.0f, row.y + row.height * 0.5f }, 4, 4.0f, 45.0f, theme::kGoldBright);
        }

        const float textY = row.y + (row.height - 28.0f) * 0.5f;
        DrawTextEx(font, items[index].label.c_str(), { row.x + 36.0f, textY }, 26.0f, 1.0f,
                   active ? theme::kGoldBright : theme::kBone);

        if (items[index].sliderValue >= 0.0f) {
            const Rectangle slider = { row.x + row.width - 180.0f, row.y + row.height * 0.5f - 2.0f, 100.0f, 4.0f };
            DrawRectangleRec(slider, Color{ 55, 48, 38, 255 });
            DrawRectangleRec({ slider.x, slider.y, slider.width * items[index].sliderValue, slider.height },
                             theme::kGold);
            DrawCircleV({ slider.x + slider.width * items[index].sliderValue, slider.y + slider.height * 0.5f },
                        6.0f, active ? theme::kGoldBright : theme::kBone);
            DrawTextEx(font, items[index].detail.c_str(),
                       { slider.x + slider.width + 14.0f, textY + 2.0f },
                       24.0f, 1.0f, active ? theme::kGold : theme::kGoldDim);
        } else if (items[index].hasStepper) {
            const Vector2 detailSize = MeasureTextEx(font, items[index].detail.c_str(), 24.0f, 1.0f);
            const float stepperWidth = std::max(200.0f, detailSize.x + 70.0f);
            const float stepperRight = row.x + row.width - 16.0f;
            const float stepperLeft = stepperRight - stepperWidth;
            const float arrowsY = textY + 1.0f;

            DrawTextEx(font, "<", { stepperLeft, arrowsY }, 26.0f, 1.0f,
                       active ? theme::kGoldBright : theme::kGoldDim);
            DrawTextEx(font, items[index].detail.c_str(),
                       { stepperLeft + (stepperWidth - detailSize.x) * 0.5f, textY + 2.0f },
                       24.0f, 1.0f, active ? theme::kGold : theme::kGoldDim);
            DrawTextEx(font, ">", { stepperRight - 16.0f, arrowsY }, 26.0f, 1.0f,
                       active ? theme::kGoldBright : theme::kGoldDim);
        } else if (!items[index].detail.empty()) {
            const Vector2 detailSize = MeasureTextEx(font, items[index].detail.c_str(), 24.0f, 1.0f);
            DrawTextEx(font, items[index].detail.c_str(),
                       { row.x + row.width - detailSize.x - 16.0f, textY + 2.0f },
                       24.0f, 1.0f, active ? theme::kGold : theme::kGoldDim);
        }
    }

    const float ornamentY = panel.y + panel.height - 18.0f;
    DrawLineEx({ centerX - 48.0f, ornamentY }, { centerX - 8.0f, ornamentY }, 1.0f, theme::kGoldDim);
    DrawPoly({ centerX, ornamentY }, 4, 4.0f, 45.0f, theme::kGold);
    DrawLineEx({ centerX + 8.0f, ornamentY }, { centerX + 48.0f, ornamentY }, 1.0f, theme::kGoldDim);
}

} // namespace ui::menu