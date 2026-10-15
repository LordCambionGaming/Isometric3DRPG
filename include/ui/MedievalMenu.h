#pragma once

#include <string>
#include <vector>
#include <raylib.h>

namespace ui::menu {

struct MenuItem {
    std::string label;
    std::string detail;
    float sliderValue = -1.0f;
    bool hasStepper = false;
};

Rectangle OptionBounds(int index, int count, const char* subtitle = nullptr);
Rectangle SliderBounds(int index, int count, const char* subtitle = nullptr);
Rectangle StepperLeftArrowBounds(int index, int count, const char* subtitle = nullptr);
Rectangle StepperRightArrowBounds(int index, int count, const char* subtitle = nullptr);
int OptionAt(Vector2 point, int count, const char* subtitle = nullptr);
void Draw(const char* title, const char* subtitle, const std::vector<MenuItem>& items, int selected,
          bool useTitleFont = false);

}