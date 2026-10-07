#include "ui/HUD.h"
#include "entities/Player.h"
#include "ui/GothicTheme.h"
#include "core/Localization.h"
#include "rendering/LightingSystem.h"

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
void HUD::Render(const Player& player, const Camera3D& camera, const core::LightingSystem* lighting)
{
    if (!m_enabled) return;

    // 1. Indicatore player (chevron sopra la testa o freccia ai bordi)
    DrawPlayerIndicator(player.GetPosition(),
                        player.GetIndicatorAnchor(),
                        camera);

    // 2. Pannello personaggio in basso a sinistra
    DrawCharacterPanel(player.GetCharacter());

    // 3. Bussola in alto a destra
    DrawCompass(camera);

    // 4. Orologio solare in alto a destra (sotto la bussola)
    float timeOfDay = lighting ? lighting->GetTimeOfDay() : 0.3f;
    DrawDayClock(timeOfDay);
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
    const float panelH  = 180.0f;
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
    DrawText(lang::Get("hud.hero").c_str(), (int)(px + 16), (int)(py + 12), 22, ui::theme::kBone);

    const std::string className = lang::CharacterClassName(core::ToString(c.characterClass));
    const char* subtitle = TextFormat(lang::Get("hud.class_level").c_str(), className.c_str(),
        lang::Get("hud.level_word").c_str(), c.level);
    DrawText(subtitle, (int)(px + 16), (int)(py + 38), 18, ui::theme::kGoldDim);
    DrawText(TextFormat(lang::Get("hud.gold").c_str(), c.gold),
             (int)(px + 16), (int)(py + 58), 18, ui::theme::kGoldBright);

    // ── Barra vita (animata + flash) ────────────────────
    DrawHealthBar(c,
                  px + 16.0f,
                  py + 80.0f,
                  panelW - 32.0f,
                  22.0f);

    // ── Arma equipaggiata ───────────────────────────────
    std::string weaponName = lang::Get("hud.unarmed");
    if (c.HasEquippedWeapon()) {
        const core::WeaponDef weapon = c.ActiveWeapon();
        if (const core::ItemDef* item = core::GetItem(weapon.id)) weaponName = lang::ItemName(item->key);
    }
    DrawText(TextFormat(lang::Get("hud.weapon").c_str(), weaponName.c_str()),
             (int)(px + 16), (int)(py + 122), 18, ui::theme::kBone);
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
    const char* label = TextFormat(lang::Get("hud.hp").c_str(), hp, maxHp);
    const int fontSize = 18;
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
    const int fontSize = 18;
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

// ──────────────────────────────────────────────────────────────
// Bussola premium — rosa dei venti con ago a diamante
// ──────────────────────────────────────────────────────────────
void HUD::DrawCompass(const Camera3D& camera) const
{
    const float sw = static_cast<float>(GetScreenWidth());
    const float cx = sw - 68.0f;
    const float cy = 72.0f;
    const float Ro = 44.0f;   // anello esterno
    const float Ri = 36.0f;   // anello interno / quadrante

    // Direzione avanti camera in XZ → angolo Nord
    Vector3 fwd = Vector3Subtract(camera.target, camera.position);
    fwd.y = 0.0f;
    const float fwdLen = sqrtf(fwd.x * fwd.x + fwd.z * fwd.z);
    if (fwdLen > 0.0001f) { fwd.x /= fwdLen; fwd.z /= fwdLen; }
    // northAngle: 0 = nord punta in alto sullo schermo
    const float northAngle = atan2f(-fwd.x, -fwd.z);

    // ── GLOW esterno
    DrawCircle((int)cx, (int)cy, (int)(Ro + 6), Fade(Color{120, 80, 20, 255}, 0.18f));
    DrawCircle((int)cx, (int)cy, (int)(Ro + 3), Fade(Color{160,110, 30, 255}, 0.25f));

    // ── Anello esterno doppio (oro)
    DrawRing({ cx, cy }, Ro - 1.0f, Ro + 1.0f, 0.0f, 360.0f, 72, ui::theme::kGold);
    DrawRing({ cx, cy }, Ro - 4.0f, Ro - 2.5f, 0.0f, 360.0f, 72, Fade(ui::theme::kGoldDim, 0.6f));

    // ── Sfondo quadrante (radiale scuro → molto scuro)
    DrawCircle((int)cx, (int)cy, (int)Ri, Color{ 8, 6, 12, 240 });
    DrawCircle((int)cx, (int)cy, (int)(Ri * 0.55f), Color{ 14, 11, 20, 200 });

    // ── 36 Tacche attorno al quadrante
    for (int i = 0; i < 36; ++i) {
        const float a = northAngle + i * (PI * 2.0f / 36.0f);
        const bool cardinal  = (i % 9 == 0);  // N/E/S/W
        const bool ordinal   = (i % 9 != 0) && (i % 3 == 0); // NE/SE/SW/NW + mezze
        const float outerR = Ri - 1.0f;
        const float innerR = cardinal ? Ri - 11.0f : (ordinal ? Ri - 8.0f : Ri - 5.0f);
        const float thick   = cardinal ? 2.2f : (ordinal ? 1.6f : 0.9f);
        Color col = cardinal
            ? ui::theme::kGoldBright
            : (ordinal ? ui::theme::kGoldDim : Fade(ui::theme::kBone, 0.3f));
        DrawLineEx(
            { cx + cosf(a) * innerR, cy + sinf(a) * innerR },
            { cx + cosf(a) * outerR, cy + sinf(a) * outerR },
            thick, col);
    }

    // ── Etichette cardinali (N E S W)
    const std::string labels[] = {
        lang::Get("compass.north"), lang::Get("compass.east"),
        lang::Get("compass.south"), lang::Get("compass.west")
    };
    for (int i = 0; i < 4; ++i) {
        const float a = northAngle + i * (PI * 0.5f);
        const float lx = cx + sinf(a + PI * 0.5f) * (Ri - 17.0f);  // offset corretto per i/e/s/o
        // N punta nella direzione northAngle - PI*0.5 (su schermo = su)
        const float la = northAngle - PI * 0.5f + i * (PI * 0.5f);
        const float lX = cx + cosf(la) * (Ri - 17.0f);
        const float lY = cy + sinf(la) * (Ri - 17.0f);
        const Color lc = (i == 0) ? Color{255, 80, 60, 255} : ui::theme::kGoldBright;
        const int fs = (i == 0) ? 13 : 10;
        DrawText(labels[i].c_str(), (int)(lX - (i == 0 ? 4 : 3)), (int)(lY - fs / 2), fs, lc);
    }

    // ── Ago a doppio diamante
    // Nord: punta rossa lunga
    const float northTipA = northAngle - PI * 0.5f;
    const float southTipA = northAngle + PI * 0.5f;
    const float longArm  = Ri - 7.0f;
    const float shortArm = Ri * 0.45f;
    const float waist    = 4.5f;

    const Vector2 tipN  = { cx + cosf(northTipA) * longArm,  cy + sinf(northTipA) * longArm  };
    const Vector2 tipS  = { cx + cosf(southTipA) * shortArm, cy + sinf(southTipA) * shortArm };
    const float perpA = northTipA + PI * 0.5f;
    const Vector2 midL  = { cx + cosf(perpA) * waist, cy + sinf(perpA) * waist };
    const Vector2 midR  = { cx - cosf(perpA) * waist, cy - sinf(perpA) * waist };

    // ombra
    const Vector2 sh = {1.5f, 1.5f};
    DrawTriangleCCW({ tipN.x+sh.x, tipN.y+sh.y }, { midL.x+sh.x, midL.y+sh.y }, { midR.x+sh.x, midR.y+sh.y }, Fade(BLACK,0.55f));
    DrawTriangleCCW({ tipS.x+sh.x, tipS.y+sh.y }, { midR.x+sh.x, midR.y+sh.y }, { midL.x+sh.x, midL.y+sh.y }, Fade(BLACK,0.55f));
    // nord (rosso vivo con highlight)
    DrawTriangleCCW(tipN, midL, midR, Color{ 200, 35, 35, 255 });
    DrawTriangleCCW(tipN, midL, { cx, cy }, Color{ 230, 70, 50, 255 }); // highlight lato sx
    // sud (argento)
    DrawTriangleCCW(tipS, midR, midL, Color{ 120, 120, 140, 230 });
    DrawTriangleCCW(tipS, midR, { cx, cy }, Color{ 155, 155, 175, 230 });

    // ── Perno centrale con gem
    DrawCircle((int)cx, (int)cy, 7, Color{ 30, 25, 40, 255 });
    DrawCircle((int)cx, (int)cy, 6, Color{ 60, 50, 80, 255 });
    DrawCircle((int)cx, (int)cy, 4, ui::theme::kGold);
    DrawCircle((int)cx, (int)cy, 2, Color{ 255, 200, 80, 255 });
    DrawCircle((int)cx, (int)cy, 1, WHITE);
}

// ──────────────────────────────────────────────────────────────
// Orologio solare/lunare premium
// 0.0=mezzanotte, 0.25=alba, 0.5=mezzogiorno, 0.75=tramonto
// ORBITA: mezzanotte=basso, alba=sinistra, mezzogiorno=alto, tramonto=destra
// ──────────────────────────────────────────────────────────────
void HUD::DrawDayClock(float timeOfDay) const
{
    const float sw = static_cast<float>(GetScreenWidth());
    const float cx = sw - 68.0f;
    const float cy = 196.0f;
    const float Ro = 42.0f;   // raggio orbita
    const float Ri = 34.0f;   // raggio quadrante

    // ── Angolo orbita corretto ──
    // mezzanotte=basso(PI/2), alba=sinistra(PI), mezzogiorno=alto(-PI/2), tramonto=destra(0)
    const float orbitAngle = timeOfDay * 2.0f * PI + PI * 0.5f;
    const float bodyX = cx + cosf(orbitAngle) * Ro;
    const float bodyY = cy + sinf(orbitAngle) * Ro;
    const bool isDay   = (timeOfDay >= 0.22f && timeOfDay <= 0.78f);
    const bool isSunrise = (timeOfDay >= 0.22f && timeOfDay <= 0.35f);
    const bool isSunset  = (timeOfDay >= 0.65f && timeOfDay <= 0.78f);
    const bool isNight   = !isDay;

    // ── 1. Glow esterno
    DrawCircle((int)cx, (int)cy, (int)(Ro + 7), Fade(Color{20, 15, 40, 255}, 0.22f));
    DrawCircle((int)cx, (int)cy, (int)(Ro + 4), Fade(Color{30, 20, 50, 255}, 0.30f));

    // ── 2. Traccia orbitale (arco giorno arancio, arco notte blu)
    DrawRing({ cx, cy }, Ro - 1.5f, Ro + 1.5f, 180.0f, 360.0f, 60, Color{ 255, 160, 40, 90  });  // giorno (metà sup)
    DrawRing({ cx, cy }, Ro - 1.5f, Ro + 1.5f,   0.0f, 180.0f, 60, Color{  30,  50,130, 90  });  // notte  (metà inf)

    // ── 3. Sfondo cielo quadrante
    Color skyTop, skyBot;
    if (timeOfDay < 0.22f || timeOfDay > 0.78f) {                     // notte
        skyTop = Color{  8, 10, 38, 240 }; skyBot = Color{ 5, 6, 25, 240 };
    } else if (isSunrise) {                                             // alba
        float t = (timeOfDay - 0.22f) / 0.13f;
        skyTop = Color{ (unsigned char)(8+int(t*52)),  (unsigned char)(10+int(t*80)),  (unsigned char)(38+int(t*162)), 240 };
        skyBot = Color{ (unsigned char)(int(t*200)), (unsigned char)(int(t*80)), (unsigned char)(int(t*20)), 240 };
    } else if (isSunset) {                                              // tramonto
        float t = (timeOfDay - 0.65f) / 0.13f;
        skyTop = Color{ (unsigned char)(60-int(t*52)), (unsigned char)(120-int(t*110)),(unsigned char)(200-int(t*162)), 240 };
        skyBot = Color{ (unsigned char)(200-int(t*200)), (unsigned char)(80-int(t*80)),(unsigned char)(20), 240 };
    } else {                                                            // giorno
        skyTop = Color{ 60, 120, 200, 240 }; skyBot = Color{ 110, 170, 230, 240 };
    }
    // gradiente manuale top → bot
    for (int y = -(int)Ri; y <= (int)Ri; ++y) {
        const float t = (y + Ri) / (2.0f * Ri);
        Color row = {
            (unsigned char)(skyTop.r + int(t * (skyBot.r - skyTop.r))),
            (unsigned char)(skyTop.g + int(t * (skyBot.g - skyTop.g))),
            (unsigned char)(skyTop.b + int(t * (skyBot.b - skyTop.b))), 240 };
        const float hw = sqrtf(std::max(0.0f, Ri * Ri - (float)(y * y)));
        DrawRectangle((int)(cx - hw), (int)(cy + y), (int)(hw * 2.0f), 1, row);
    }

    // ── 4. Stelle notturne (con sfarfallio basato su frame)
    if (isNight || isSunrise || isSunset) {
        float alpha;
        if (isNight)         alpha = 0.9f;
        else if (isSunrise)  alpha = 1.0f - (timeOfDay - 0.22f) / 0.13f;
        else                 alpha = (timeOfDay - 0.65f) / 0.13f;

        struct Star { float x, y; float s; int phase; };
        static const Star stars[] = {
            {-12,-18,1.2f,0},{9,-22,1.0f,3},{-6,-10,0.8f,7},{14,-8,1.1f,2},
            {-20,-7,0.9f,5},{2,-26,1.3f,1},{20,-20,1.0f,6},{-16,-24,0.8f,4},
            {7,-14,1.2f,8},{-8,-28,0.9f,2},{17,-13,1.0f,0},{-24,-15,1.1f,5}
        };
        const float time = static_cast<float>(GetTime());
        for (const auto& s : stars) {
            const float sx = cx + s.x, sy = cy + s.y;
            if (sx < cx - Ri || sx > cx + Ri || sy < cy - Ri || sy > cy + Ri) continue;
            const float flicker = 0.6f + 0.4f * sinf(time * 3.0f + s.phase * 1.3f);
            DrawCircle((int)sx, (int)sy, (int)(s.s + 0.5f),
                       Fade(WHITE, alpha * flicker * 0.9f));
            if (s.s > 1.0f)
                DrawCircle((int)sx, (int)sy, 1, Fade(WHITE, alpha * flicker * 0.4f));
        }
    }

    // ── 5. Silhouette montagne (metà bassa del quadrante)
    {
        Color mountCol = { (unsigned char)(skyBot.r/3), (unsigned char)(skyBot.g/3),
                           (unsigned char)(skyBot.b/3 + 8), 220 };
        for (int x = -(int)Ri; x <= (int)Ri; ++x) {
            // Semplice funzione altezza per le montagne (somma di seni)
            float fx = (float)x;
            float mountY = 12.0f - (sinf(fx * 0.2f) * 4.0f + cosf(fx * 0.4f + 1.0f) * 3.0f + sinf(fx * 0.1f) * 5.0f);
            
            float bottomY = sqrtf(std::max(0.0f, Ri * Ri - fx * fx)); // bordo inferiore del cerchio
            if (mountY < bottomY) {
                DrawLine((int)(cx + x), (int)(cy + mountY), (int)(cx + x), (int)(cy + bottomY), mountCol);
            }
        }
    }

    // ── 7. Glow atmosferico attorno al corpo
    if (isDay) {
        const float glowR = isSunrise || isSunset ? 18.0f : 14.0f;
        Color glowCol = isSunrise ? Color{255,150,50,255}
                      : isSunset  ? Color{255,100,30,255}
                                  : Color{255,230,100,255};
        DrawCircle((int)bodyX, (int)bodyY, (int)(glowR + 5), Fade(glowCol, 0.12f));
        DrawCircle((int)bodyX, (int)bodyY, (int)(glowR + 2), Fade(glowCol, 0.22f));
        DrawCircle((int)bodyX, (int)bodyY, (int)glowR,        Fade(glowCol, 0.35f));
    } else {
        DrawCircle((int)bodyX, (int)bodyY, 12, Fade(Color{200, 210, 255, 255}, 0.15f));
        DrawCircle((int)bodyX, (int)bodyY,  9, Fade(Color{200, 210, 255, 255}, 0.25f));
    }

    // ── 8. Sole o Luna
    if (isDay) {
        // raggi: 12 (lunghi alternati corti)
        const float sunR = 7.0f;
        const float time = static_cast<float>(GetTime());
        for (int i = 0; i < 12; ++i) {
            const float ra = i * (PI / 6.0f) + time * 0.12f;
            const float inner = sunR + 1.5f;
            const float outer = sunR + (i % 2 == 0 ? 5.5f : 3.0f);
            DrawLineEx(
                { bodyX + cosf(ra) * inner, bodyY + sinf(ra) * inner },
                { bodyX + cosf(ra) * outer, bodyY + sinf(ra) * outer },
                i % 2 == 0 ? 1.8f : 1.2f,
                Color{255, 230, 80, 220});
        }
        // nucleo
        DrawCircle((int)bodyX, (int)bodyY, (int)sunR + 1, Color{255, 200, 60, 255});
        DrawCircle((int)bodyX, (int)bodyY, (int)sunR,     Color{255, 230, 90, 255});
        DrawCircle((int)bodyX, (int)bodyY, (int)(sunR - 2), Color{255, 250, 180, 255});
        DrawCircle((int)bodyX, (int)bodyY, 2, WHITE);
    } else {
        // Luna con crescent e crateri
        DrawCircle((int)bodyX, (int)bodyY, 7, Color{210, 215, 230, 255});
        DrawCircle((int)bodyX, (int)bodyY, 6, Color{230, 235, 250, 255});
        // crescent overlay (colore cielo)
        DrawCircle((int)(bodyX + 4), (int)bodyY, 6, skyTop);
        // crateri
        DrawCircle((int)(bodyX - 2), (int)(bodyY + 1), 1, Fade(Color{150,150,170,255}, 0.6f));
        DrawCircle((int)(bodyX + 0), (int)(bodyY - 2), 1, Fade(Color{150,150,170,255}, 0.5f));
    }

    // ── 9. Cornici (doppio anello)
    DrawRing({ cx, cy }, Ri - 1.0f, Ri + 1.0f, 0.0f, 360.0f, 72, ui::theme::kGoldDim);
    DrawRing({ cx, cy }, Ro - 1.0f, Ro + 1.0f, 0.0f, 360.0f, 72, Fade(ui::theme::kGold, 0.7f));
    DrawRing({ cx, cy }, Ro + 2.0f, Ro + 4.0f, 0.0f, 360.0f, 72, Fade(ui::theme::kGoldDim, 0.35f));
}

