#include "ui/HUD.h"

#include "player/Player.h"

#include <raymath.h>
#include <cmath>
#include <algorithm>
#include <utility>

namespace
{
    constexpr float EDGE_MARGIN = 48.0f;

    constexpr float V_OFFSET_Y = 42.0f;
    constexpr float V_HALF_WIDTH = 22.0f;
    constexpr float V_HEIGHT = 24.0f;
    constexpr float V_WIDTH = 6.0f;

    constexpr float ARROW_LENGTH = 22.0f;
    constexpr float ARROW_BASE = 14.0f;
    constexpr float ARROW_HALF_WIDTH = 11.0f;

    bool Finite(Vector2 p)
    {
        return std::isfinite(p.x) && std::isfinite(p.y);
    }

    Vector2 Normalize2(Vector2 v)
    {
        const float len = std::sqrt(v.x * v.x + v.y * v.y);
        if (len < 0.0001f)
            return {0.0f, -1.0f};
        return {v.x / len, v.y / len};
    }

    // raylib's DrawTriangle culls triangles whose vertices are not in
    // counter-clockwise order (as seen on screen, Y pointing down).
    // This helper checks the winding and swaps two vertices when needed,
    // so the triangle is drawn whatever direction the arrow points to.
    void DrawTriangleCCW(Vector2 a, Vector2 b, Vector2 c, Color color)
    {
        const float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        if (cross > 0.0f)
            std::swap(b, c);
        DrawTriangle(a, b, c, color);
    }
}

void HUD::Render(const Player& player, const Camera3D& camera)
{
    if (!m_enabled)
        return;

    DrawPlayerIndicator(player.GetPosition(), camera);
}

void HUD::DrawPlayerIndicator(const Vector3& playerPosition, const Camera3D& camera) const
{
    const float sw = static_cast<float>(GetScreenWidth());
    const float sh = static_cast<float>(GetScreenHeight());

    if (sw < 200.0f || sh < 150.0f)
        return;

    const Vector2 projected = GetWorldToScreen(playerPosition, camera);
    const float cx = sw * 0.5f;
    const float cy = sh * 0.5f;

    // Determine whether the world point is actually in front of the camera.
    Vector3 forward = Vector3Subtract(camera.target, camera.position);
    const float forwardLen = Vector3Length(forward);
    if (forwardLen > 0.0001f)
        forward = Vector3Scale(forward, 1.0f / forwardLen);
    else
        forward = {0.0f, 0.0f, -1.0f};

    const Vector3 toPlayer = Vector3Subtract(playerPosition, camera.position);
    const bool inFront = Vector3DotProduct(toPlayer, forward) > 0.0f;

    const bool inside =
        Finite(projected) &&
        projected.x >= 0.0f && projected.x <= sw &&
        projected.y >= 0.0f && projected.y <= sh;

    // ------------------------------
    // PLAYER IS IN CAMERA
    // ------------------------------
    if (inFront && inside)
    {
        const float x = projected.x;
        const float y = projected.y - V_OFFSET_Y;

        const Vector2 a = {x - V_HALF_WIDTH, y - V_HEIGHT};
        const Vector2 b = {x, y};
        const Vector2 c = {x + V_HALF_WIDTH, y - V_HEIGHT};

        DrawLineEx(a, b, V_WIDTH + 3.0f, BLACK);
        DrawLineEx(b, c, V_WIDTH + 3.0f, BLACK);
        DrawLineEx(a, b, V_WIDTH, YELLOW);
        DrawLineEx(b, c, V_WIDTH, YELLOW);
        return;
    }

    // ------------------------------
    // PLAYER IS OUTSIDE CAMERA
    // ------------------------------
    // For normal off-screen points GetWorldToScreen() gives exactly the
    // screen-space direction we need. For a point behind the camera the
    // projected value can be mirrored, so flip that vector.
    Vector2 direction;

    if (Finite(projected))
    {
        direction = {projected.x - cx, projected.y - cy};
    }
    else
    {
        // Fallback to camera-space basis if projection is non-finite.
        Vector3 up = camera.up;
        const float upLen = Vector3Length(up);
        if (upLen > 0.0001f)
            up = Vector3Scale(up, 1.0f / upLen);
        else
            up = {0.0f, 1.0f, 0.0f};

        Vector3 right3 = Vector3CrossProduct(forward, up);
        const float rightLen = Vector3Length(right3);
        if (rightLen > 0.0001f)
            right3 = Vector3Scale(right3, 1.0f / rightLen);
        else
            right3 = {1.0f, 0.0f, 0.0f};

        direction = {
            Vector3DotProduct(toPlayer, right3),
            -Vector3DotProduct(toPlayer, up)
        };
    }

    if (!inFront)
    {
        direction.x = -direction.x;
        direction.y = -direction.y;
    }

    direction = Normalize2(direction);

    // Start from screen center and find the intersection with the inner
    // rectangle. This always gives a visible point.
    const float left = EDGE_MARGIN;
    const float right = sw - EDGE_MARGIN;
    const float top = EDGE_MARGIN;
    const float bottom = sh - EDGE_MARGIN;

    float t = 1000000.0f;

    if (std::abs(direction.x) > 0.0001f)
    {
        const float edgeX = direction.x > 0.0f ? right : left;
        const float candidate = (edgeX - cx) / direction.x;

        if (candidate > 0.0f)
        {
            const float y = cy + direction.y * candidate;
            if (y >= top && y <= bottom)
                t = std::min(t, candidate);
        }
    }

    if (std::abs(direction.y) > 0.0001f)
    {
        const float edgeY = direction.y > 0.0f ? bottom : top;
        const float candidate = (edgeY - cy) / direction.y;

        if (candidate > 0.0f)
        {
            const float x = cx + direction.x * candidate;
            if (x >= left && x <= right)
                t = std::min(t, candidate);
        }
    }

    if (t > 999999.0f)
        t = std::min(sw, sh) * 0.35f;

    const Vector2 edge = {
        cx + direction.x * t,
        cy + direction.y * t
    };

    // Keep the complete triangle comfortably inside the viewport.
    const Vector2 tip = {
        edge.x + direction.x * ARROW_LENGTH,
        edge.y + direction.y * ARROW_LENGTH
    };

    const Vector2 baseCenter = {
        edge.x - direction.x * ARROW_BASE,
        edge.y - direction.y * ARROW_BASE
    };

    const Vector2 perp = {
        -direction.y,
        direction.x
    };

    const Vector2 baseA = {
        baseCenter.x + perp.x * ARROW_HALF_WIDTH,
        baseCenter.y + perp.y * ARROW_HALF_WIDTH
    };

    const Vector2 baseB = {
        baseCenter.x - perp.x * ARROW_HALF_WIDTH,
        baseCenter.y - perp.y * ARROW_HALF_WIDTH
    };

    // Shadow, then the arrow itself (winding handled by DrawTriangleCCW).
    DrawTriangleCCW(
        {tip.x + 2.0f, tip.y + 2.0f},
        {baseA.x + 2.0f, baseA.y + 2.0f},
        {baseB.x + 2.0f, baseB.y + 2.0f},
        BLACK
    );

    DrawTriangleCCW(tip, baseA, baseB, YELLOW);
}