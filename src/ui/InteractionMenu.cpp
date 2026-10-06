#include "ui/InteractionMenu.h"
#include <algorithm>
#include <cmath>

namespace ui {

// ── Helpers ──────────────────────────────────────────────────────────────────

static void DrawRoundedRectFilled(Rectangle r, float rnd, Color fill) {
    DrawRectangleRounded(r, rnd / std::max(r.width, r.height), 8, fill);
}
static void DrawRoundedRectLines(Rectangle r, float rnd, Color col) {
    DrawRectangleRoundedLines(r, rnd / std::max(r.width, r.height), 8, col);
}

// ── InteractionMenu ───────────────────────────────────────────────────────────

InteractionMenu::InteractionMenu() = default;

void InteractionMenu::Open(const Interactable& target) {
    m_target = target;
    m_open   = true;
    m_hoveredIndex = -1;
}

void InteractionMenu::Close() {
    m_open = false;
}

bool InteractionMenu::Update() {
    if (!m_open) return false;

    // Close on ESC
    if (IsKeyPressed(KEY_ESCAPE)) { Close(); return true; }

    // Detect click
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 mouse = GetMousePosition();
        if (!CheckCollisionPointRec(mouse, m_bounds)) {
            // Click landed outside, close the menu and let world handle the click
            Close();
            return false; 
        } else {
            // Click inside panel — check items
            int ny = (int)m_bounds.y + kHeaderH + kPadY;
            for (int i = 0; i < (int)m_target.actions.size(); ++i) {
                Rectangle itemRect = { m_bounds.x + 4, (float)ny, m_bounds.width - 8, (float)kItemH };
                if (CheckCollisionPointRec(mouse, itemRect)) {
                    if (m_target.actions[i].callback) m_target.actions[i].callback();
                    Close();
                    return true;
                }
                ny += kItemH;
            }
            return true; // Clicked inside the menu but not on an action, still consumes click
        }
    }

    return false;
}

void InteractionMenu::Draw(const Camera3D& camera) const {
    if (!m_open) return;

    // Project entity head to screen space
    // We offset upward by 1.8 units (approx head height)
    Vector3 headPos = { m_target.worldPosition.x,
                        m_target.worldPosition.y + 1.8f,
                        m_target.worldPosition.z };
    Vector2 screenPos = GetWorldToScreen(headPos, camera);

    // ── Layout ────────────────────────────────────────────────────────────────
    int numActions = (int)m_target.actions.size();

    // Compute required width from longest label
    int maxLabelW = MeasureText(m_target.name.c_str(), 13);
    for (const auto& act : m_target.actions)
        maxLabelW = std::max(maxLabelW, MeasureText(act.label.c_str(), 13));

    int panelW = std::max(kMinWidth, maxLabelW + kPadX * 2);
    int panelH = kHeaderH + kPadY + numActions * kItemH + kPadY;

    // Position the panel centered above screen-projected point
    int px = (int)screenPos.x - panelW / 2;
    int py = (int)screenPos.y - panelH - 10; // 10px above head dot

    // Clamp to screen
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    if (px < 4) px = 4;
    if (px + panelW > sw - 4) px = sw - 4 - panelW;
    if (py < 4) py = 4;
    if (py + panelH > sh - 4) py = sh - 4 - panelH;

    m_bounds = { (float)px, (float)py, (float)panelW, (float)panelH };

    // ── Draw panel shadow ─────────────────────────────────────────────────────
    Rectangle shadowR = { m_bounds.x + 3, m_bounds.y + 4, m_bounds.width, m_bounds.height };
    DrawRoundedRectFilled(shadowR, kCorner, Color{0,0,0,80});

    // ── Draw panel background ─────────────────────────────────────────────────
    // Dark semi-transparent gothic body
    DrawRoundedRectFilled(m_bounds, kCorner, Color{18, 14, 22, 235});
    // Thin accent border (gold)
    DrawRoundedRectLines(m_bounds, kCorner, Color{190, 150, 60, 220});

    // ── Header ────────────────────────────────────────────────────────────────
    Rectangle headerR = { m_bounds.x, m_bounds.y, m_bounds.width, (float)kHeaderH };
    DrawRoundedRectFilled(headerR, kCorner, Color{40, 20, 50, 255});
    // Gold bottom line under header
    DrawLineEx({ m_bounds.x + 6, m_bounds.y + kHeaderH },
               { m_bounds.x + m_bounds.width - 6, m_bounds.y + kHeaderH },
               1.0f, Color{190, 150, 60, 180});

    int nameW = MeasureText(m_target.name.c_str(), 13);
    int nameX = px + (panelW - nameW) / 2;
    DrawText(m_target.name.c_str(), nameX, py + 5, 13, Color{255, 215, 100, 255});

    // ── Action items ──────────────────────────────────────────────────────────
    Vector2 mouse = GetMousePosition();
    int ny = py + kHeaderH + kPadY;

    m_hoveredIndex = -1;
    for (int i = 0; i < numActions; ++i) {
        Rectangle itemR = { (float)(px + 4), (float)ny, (float)(panelW - 8), (float)kItemH };
        bool hov = CheckCollisionPointRec(mouse, itemR);
        if (hov) m_hoveredIndex = i;

        if (hov) {
            DrawRoundedRectFilled(itemR, 4.0f, Color{80, 50, 100, 200});
            DrawRoundedRectLines(itemR, 4.0f, Color{190, 150, 60, 120});
        }

        // Small decorative diamond bullet
        int bulletX = px + 10;
        int bulletY = ny + kItemH / 2;
        DrawPoly({(float)bulletX, (float)bulletY}, 4, 4, 45, hov ? Color{255,215,100,255} : Color{150,120,60,200});

        int labelX = px + 22;
        int labelY = ny + (kItemH - 13) / 2;
        DrawText(m_target.actions[i].label.c_str(), labelX, labelY, 13,
                 hov ? Color{255,240,180,255} : Color{200,185,160,230});

        ny += kItemH;
    }

    // ── Connector line from panel to entity ───────────────────────────────────
    Vector2 panelBottom = { screenPos.x, (float)(py + panelH) };
    DrawLineEx(panelBottom, { screenPos.x, screenPos.y - 2 }, 1.2f, Color{190, 150, 60, 140});
    DrawCircleV({ screenPos.x, screenPos.y }, 3.0f, Color{255, 215, 100, 200});
}

} // namespace ui

