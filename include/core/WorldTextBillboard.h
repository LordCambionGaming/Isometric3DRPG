#pragma once
#include <raylib.h>
#include <cstring>

inline void DrawWorldText(const char* text, Vector3 worldPos, const Camera3D& cam, float size, Color color) {
    Vector2 screen = GetWorldToScreen(worldPos, cam);
    int len = (int)strlen(text);
    float charW = size * 0.6f;
    float totalW = len * charW;
    float x = screen.x - totalW * 0.5f;
    float y = screen.y - size * 0.5f;
    // Small quad facing camera (billboard orientation via screen projection)
    DrawRectangle((int)(x - size*0.6f), (int)(y - size*0.3f), (int)(totalW + size*1.2f), (int)(size*1.4f), Fade(BLACK, 0.55f));
    DrawText(text, (int)x, (int)y, (int)size, color);
}
