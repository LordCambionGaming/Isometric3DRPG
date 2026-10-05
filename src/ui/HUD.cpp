#include "ui/HUD.h"
#include "entities/Player.h"
#include "ui/GothicTheme.h"

#include <raymath.h>
#include <cmath>
#include <algorithm>
#include <utility>

// ──────────────────────────────────────────────────────────────
// Namespace anonimo: helper locali (non esportati dall'header)
// ──────────────────────────────────────────────────────────────
namespace
{
    // Palette rapida dal tema gotico
    constexpr Color kHPFill    { 180,  30,  40, 255 };  // rosso sangue
    constexpr Color kHPBack    {  40,  12,  16, 255 };  // rosso scurissimo
    constexpr Color kMPFill    {  60, 100, 200, 255 };  // blu mana
    constexpr Color kMPBack    {  16,  22,  46, 255 };  // blu scurissimo
    constexpr Color kStamFill  {  90, 170,  80, 255 };  // verde stamina
    constexpr Color kStamBack  {  20,  40,  18, 255 };  // verde scuro
    constexpr Color kBarBorder { 212, 175,  90, 255 };  // oro (kGold di GothicTheme)
    constexpr Color kBarShadow {   0,   0,   0, 255 };
    constexpr Color kText      { 226, 216, 196, 255 };  // kBone

    // Margini di sicurezza per lo schermo
    constexpr float EDGE_MARGIN = 48.0f;

    // Indicator player (invariato dal tuo codice)
    constexpr float V_OFFSET_Y = 4.0f;
    constexpr float V_HALF_WIDTH = 22.0f;
    constexpr float V_HEIGHT = 24.0f;
    constexpr float V_WIDTH = 6.0f;
    constexpr float ARROW_LENGTH = 22.0f;
    constexpr float ARROW_BASE = 14.0f;
    constexpr float ARROW_HALF_WIDTH = 11.0f;

    bool Finite(Vector2 p) {
        return std::isfinite(p.x) && std::isfinite(p.y);
    }

    Vector2 Normalize2(Vector2 v) {
        const float len = std::sqrt(v.x * v.x + v.y * v.y);
        if (len < 0.0001f) return { 0.0f, -1.0f };
        return { v.x / len, v.y / len };
    }

    void DrawTriangleCCW(Vector2 a, Vector2 b, Vector2 c, Color color) {
        const float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        if (cross > 0.0f) std::swap(b, c);
        DrawTriangle(a, b, c, color);
    }

    // Colore riempimento in base alla frazione (rosso scuro → rosso → giallo → verde)
    Color HealthColorFor(float frac) {
        if (frac < 0.25f)      return Color{ 200, 30, 30, 255 };   // critico
        else if (frac < 0.5f)  return Color{ 200, 90, 40, 255 };   // ferito
        else                   return kHPFill;
    }
}

// ──────────────────────────────────────────────────────────────
// HUD::Render — entry point chiamato da Game::Render
// ──────────────────────────────────────────────────────────────
void HUD::Render(const Player& player, const Camera3D& camera)
{
    if (!m_enabled) return;

    // 1. Indicatore player (chevron sopra la testa o freccia ai bordi)
    DrawPlayerIndicator(player.GetPosition(),
                        player.GetIndicatorAnchor(),
                        camera);

    // 2. Pannello personaggio in basso a sinistra
    DrawCharacterPanel(player.GetCharacter());
}

// ──────────────────────────────────────────────────────────────
// Pannello personaggio (nome, classe, livello, HP, arma)
// ──────────────────────────────────────────────────────────────
void HUD::DrawCharacterPanel(const core::Character& c) const
{
    const int sw = GetScreenWidth();
    const int sh = GetScreenHeight();

    // Layout: pannello in basso a sinistra
    const float margin  = 16.0f;
    const float panelW  = 320.0f;
    const float panelH  = 150.0f;
    const float px      = margin;
    const float py      = sh - margin - panelH;

    // ── Sfondo pannello ─────────────────────────────────
    DrawRectangle((int)px, (int)py, (int)panelW, (int)panelH,
                  Fade(ui::theme::kIron, 0.85f));

    // Doppio bordo stile gotico
    DrawRectangleLines((int)px, (int)py, (int)panelW, (int)panelH,
                       ui::theme::kGoldDim);
    DrawRectangleLines((int)(px + 4), (int)(py + 4),
                       (int)(panelW - 8), (int)(panelH - 8),
                       Fade(ui::theme::kGoldDim, 0.5f));

    // ── Nome + classe + livello ─────────────────────────
    // (il nome non esiste in Character: placeholder)
    const char* name = "Eroe";
    DrawText(name, (int)(px + 16), (int)(py + 12), 22, ui::theme::kBone);

    const char* subtitle = TextFormat("%s  ·  Livello %d",
        core::ToString(c.characterClass), c.level);
    DrawText(subtitle, (int)(px + 16), (int)(py + 38), 16, ui::theme::kGoldDim);

    // ── Barra vita (animata + flash) ────────────────────
    DrawHealthBar(c,
                  px + 16.0f,
                  py + 66.0f,
                  panelW - 32.0f,
                  22.0f);

    // ── Arma equipaggiata ───────────────────────────────
    const char* weapon = c.HasEquippedWeapon()
        ? c.ownedWeapons[c.equippedWeapon].name
        : "(disarmato)";
    DrawText(TextFormat("Arma: %s", weapon),
             (int)(px + 16), (int)(py + 108), 16, ui::theme::kBone);
}

// ──────────────────────────────────────────────────────────────
// Barra vita: legge l'HP, aggiorna l'animazione, delega il disegno
// ──────────────────────────────────────────────────────────────
void HUD::DrawHealthBar(const core::Character& c,
                        float x, float y, float w, float h) const
{
    const int   hp       = c.vitals.currentHP;
    const int   maxHp    = c.MaxHP();
    const float targetHP = c.HPFraction();
    const float dt       = GetFrameTime();

    // ── 1. Rileva danno e avvia il flash ────────────────
    if (m_lastHP >= 0 && hp < m_lastHP) {
        m_damageFlash = 1.0f;
    }
    m_lastHP = hp;

    // ── 2. Animazione esponenziale verso il target ──────
    const float lerpSpeed = 6.0f;
    m_displayedHP += (targetHP - m_displayedHP) * std::min(1.0f, lerpSpeed * dt);

    // ── 3. Decadimento del flash ────────────────────────
    m_damageFlash = std::max(0.0f, m_damageFlash - dt * 3.0f);

    // ── 4. Colore dinamico in base all'HP ───────────────
    Color fill = HealthColorFor(m_displayedHP);
    // Flash: schiarisce verso il bianco
    fill = ColorLerp(fill, WHITE, m_damageFlash * 0.7f);

    // ── 5. Disegno (fondo → alto) ───────────────────────
    // 5a. Ombra esterna
    DrawRectangle((int)(x - 2), (int)(y - 2),
                  (int)(w + 4), (int)(h + 4), kBarShadow);
    // 5b. Bordo dorato
    DrawRectangleLines((int)(x - 2), (int)(y - 2),
                       (int)(w + 4), (int)(h + 4), kBarBorder);
    // 5c. Sfondo interno (vuoto)
    DrawRectangle((int)x, (int)y, (int)w, (int)h, kHPBack);
    // 5d. Riempimento proporzionale
    const float fillW = w * m_displayedHP;
    DrawRectangle((int)x, (int)y, (int)fillW, (int)h, fill);
    // 5e. Highlight in alto (effetto luce)
    DrawRectangle((int)x, (int)y, (int)fillW, 3, Fade(WHITE, 0.35f));
    // 5f. Ombra in basso
    DrawRectangle((int)x, (int)(y + h - 3), (int)fillW, 3, Fade(BLACK, 0.35f));

    // ── 6. Testo centrato ───────────────────────────────
    const char* label = TextFormat("HP  %d / %d", hp, maxHp);
    const int fontSize = 16;
    const int textW = MeasureText(label, fontSize);
    const int textX = (int)(x + (w - textW) / 2);
    const int textY = (int)(y + (h - fontSize) / 2);
    DrawText(label, textX + 1, textY + 1, fontSize, Fade(BLACK, 0.8f));
    DrawText(label, textX, textY, fontSize, kText);

    // ── 7. Velo rosso a schermo intero durante il flash ─
    if (m_damageFlash > 0.0f) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      Fade(RED, m_damageFlash * 0.15f));
    }
}

// ──────────────────────────────────────────────────────────────
// Barra risorsa generica (HP, MP, stamina...)
// ──────────────────────────────────────────────────────────────
void HUD::DrawResourceBar(const char* label, int current, int max,
                          Color fillColor,
                          float x, float y, float w, float h) const
{
    const float frac = (max > 0)
        ? static_cast<float>(current) / static_cast<float>(max)
        : 0.0f;

    // Cornice
    DrawRectangle((int)(x - 2), (int)(y - 2),
                  (int)(w + 4), (int)(h + 4), kBarShadow);
    DrawRectangleLines((int)(x - 2), (int)(y - 2),
                       (int)(w + 4), (int)(h + 4), kBarBorder);
    // Sfondo
    DrawRectangle((int)x, (int)y, (int)w, (int)h, Color{ 20, 20, 24, 255 });
    // Riempimento
    DrawRectangle((int)x, (int)y, (int)(w * frac), (int)h, fillColor);
    // Highlight + ombra
    DrawRectangle((int)x, (int)y, (int)(w * frac), 2, Fade(WHITE, 0.3f));
    DrawRectangle((int)x, (int)(y + h - 2), (int)(w * frac), 2, Fade(BLACK, 0.3f));

    // Testo
    const char* text = TextFormat("%s  %d/%d", label, current, max);
    const int fontSize = 14;
    const int textW = MeasureText(text, fontSize);
    const int textX = (int)(x + (w - textW) / 2);
    const int textY = (int)(y + (h - fontSize) / 2);
    DrawText(text, textX + 1, textY + 1, fontSize, Fade(BLACK, 0.8f));
    DrawText(text, textX, textY, fontSize, kText);
}

// ──────────────────────────────────────────────────────────────
// Barra vita sopra un'entità del mondo 3D (nemici, alleati, ...)
// ──────────────────────────────────────────────────────────────
void HUD::DrawWorldHealthBar(Vector3 worldPos, float hpFrac,
                             const Camera3D& camera, float barWidthPx) const
{
    // 1. Proietta la posizione 3D nello schermo
    const Vector2 screenPos = GetWorldToScreen(worldPos, camera);

    // 2. Scarta se fuori dallo schermo (margine)
    const int sw = GetScreenWidth();
    const int sh = GetScreenHeight();
    if (screenPos.x < -barWidthPx || screenPos.x > sw + barWidthPx) return;
    if (screenPos.y < -20.0f       || screenPos.y > sh + 20.0f)      return;

    // 3. Geometria della barra
    const float barW = barWidthPx;
    const float barH = 6.0f;
    const float x    = screenPos.x - barW * 0.5f;
    const float y    = screenPos.y - barH * 0.5f;

    // 4. Colore in base alla frazione
    const Color fill = HealthColorFor(hpFrac);

    // 5. Disegno
    DrawRectangle((int)(x - 1), (int)(y - 1),
                  (int)(barW + 2), (int)(barH + 2), Fade(BLACK, 0.85f));
    DrawRectangle((int)x, (int)y, (int)barW, (int)barH, kHPBack);
    DrawRectangle((int)x, (int)y, (int)(barW * hpFrac), (int)barH, fill);
    // Bordo sottile bianco per staccare dallo sfondo
    DrawRectangleLines((int)(x - 1), (int)(y - 1),
                       (int)(barW + 2), (int)(barH + 2),
                       Fade(WHITE, 0.5f));
}

// ──────────────────────────────────────────────────────────────
// Indicatore player (chevron sopra la testa o freccia ai bordi)
// [CODICE INVARIATO DAL TUO HUD.cpp ORIGINALE]
// ──────────────────────────────────────────────────────────────
void HUD::DrawPlayerIndicator(const Vector3& playerPosition,
                              const Vector3& headAnchor,
                              const Camera3D& camera) const
{
    const float sw = static_cast<float>(GetScreenWidth());
    const float sh = static_cast<float>(GetScreenHeight());

    if (sw < 200.0f || sh < 150.0f) return;

    const Vector2 projected = GetWorldToScreen(playerPosition, camera);
    const float cx = sw * 0.5f;
    const float cy = sh * 0.5f;

    Vector3 forward = Vector3Subtract(camera.target, camera.position);
    const float forwardLen = Vector3Length(forward);
    if (forwardLen > 0.0001f) forward = Vector3Scale(forward, 1.0f / forwardLen);
    else forward = { 0.0f, 0.0f, -1.0f };

    const Vector3 toPlayer = Vector3Subtract(playerPosition, camera.position);
    const bool inFront = Vector3DotProduct(toPlayer, forward) > 0.0f;

    const bool inside =
        Finite(projected) &&
        projected.x >= 0.0f && projected.x <= sw &&
        projected.y >= 0.0f && projected.y <= sh;

    // ── CASO 1: player dentro la camera ────────────────
    if (inFront && inside) {
        const Vector2 head = GetWorldToScreen(headAnchor, camera);
        const float x = Finite(head) ? head.x : projected.x;
        float y = (Finite(head) ? head.y : projected.y) - V_OFFSET_Y;
        y = std::max(y, V_HEIGHT + V_WIDTH);

        const Vector2 a = { x - V_HALF_WIDTH, y - V_HEIGHT };
        const Vector2 b = { x, y };
        const Vector2 c = { x + V_HALF_WIDTH, y - V_HEIGHT };

        DrawLineEx(a, b, V_WIDTH + 3.0f, BLACK);
        DrawLineEx(b, c, V_WIDTH + 3.0f, BLACK);
        DrawLineEx(a, b, V_WIDTH, YELLOW);
        DrawLineEx(b, c, V_WIDTH, YELLOW);
        return;
    }

    // ── CASO 2: player fuori dalla camera ──────────────
    Vector2 direction;
    if (Finite(projected)) {
        direction = { projected.x - cx, projected.y - cy };
    } else {
        Vector3 up = camera.up;
        const float upLen = Vector3Length(up);
        if (upLen > 0.0001f) up = Vector3Scale(up, 1.0f / upLen);
        else up = { 0.0f, 1.0f, 0.0f };

        Vector3 right3 = Vector3CrossProduct(forward, up);
        const float rightLen = Vector3Length(right3);
        if (rightLen > 0.0001f) right3 = Vector3Scale(right3, 1.0f / rightLen);
        else right3 = { 1.0f, 0.0f, 0.0f };

        direction = {
            Vector3DotProduct(toPlayer, right3),
            -Vector3DotProduct(toPlayer, up)
        };
    }

    if (!inFront) {
        direction.x = -direction.x;
        direction.y = -direction.y;
    }

    direction = Normalize2(direction);

    const float left   = EDGE_MARGIN;
    const float right  = sw - EDGE_MARGIN;
    const float top    = EDGE_MARGIN;
    const float bottom = sh - EDGE_MARGIN;

    float t = 1000000.0f;

    if (std::abs(direction.x) > 0.0001f) {
        const float edgeX = direction.x > 0.0f ? right : left;
        const float candidate = (edgeX - cx) / direction.x;
        if (candidate > 0.0f) {
            const float y = cy + direction.y * candidate;
            if (y >= top && y <= bottom) t = std::min(t, candidate);
        }
    }

    if (std::abs(direction.y) > 0.0001f) {
        const float edgeY = direction.y > 0.0f ? bottom : top;
        const float candidate = (edgeY - cy) / direction.y;
        if (candidate > 0.0f) {
            const float x = cx + direction.x * candidate;
            if (x >= left && x <= right) t = std::min(t, candidate);
        }
    }

    if (t > 999999.0f) t = std::min(sw, sh) * 0.35f;

    const Vector2 edge = { cx + direction.x * t, cy + direction.y * t };
    const Vector2 tip  = { edge.x + direction.x * ARROW_LENGTH,
                           edge.y + direction.y * ARROW_LENGTH };
    const Vector2 baseCenter = { edge.x - direction.x * ARROW_BASE,
                                 edge.y - direction.y * ARROW_BASE };
    const Vector2 perp = { -direction.y, direction.x };
    const Vector2 baseA = { baseCenter.x + perp.x * ARROW_HALF_WIDTH,
                            baseCenter.y + perp.y * ARROW_HALF_WIDTH };
    const Vector2 baseB = { baseCenter.x - perp.x * ARROW_HALF_WIDTH,
                            baseCenter.y - perp.y * ARROW_HALF_WIDTH };

    DrawTriangleCCW(
        { tip.x + 2.0f, tip.y + 2.0f },
        { baseA.x + 2.0f, baseA.y + 2.0f },
        { baseB.x + 2.0f, baseB.y + 2.0f },
        BLACK);
    DrawTriangleCCW(tip, baseA, baseB, YELLOW);
}