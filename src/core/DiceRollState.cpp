#include "core/DiceRollState.h"
#include "core/Game.h"
#include "ui/GothicTheme.h"
#include <raylib.h>
#include <cmath>
#include <memory>

using namespace ui::theme;

namespace {

Rectangle Inset(Rectangle r, float d) {
    return { r.x + d, r.y + d, r.width - 2 * d, r.height - 2 * d };
}

// Guglie gotiche lungo il bordo superiore della board
void DrawSpires(const Rectangle& b) {
    const int n = 11;
    const float x0 = b.x + 10.0f;
    const float step = (b.width - 20.0f) / n;
    for (int i = 0; i < n; ++i) {
        const float bx = x0 + step * i;
        const float h = (i == n / 2) ? 52.0f : (i % 2 ? 18.0f : 30.0f);
        const Vector2 apex = { bx + step / 2.0f, b.y - h };
        const Vector2 bl = { bx, b.y + 2.0f };
        const Vector2 br = { bx + step, b.y + 2.0f };
        DrawTriangle(apex, bl, br, kIron);
        DrawTriangleLines(apex, bl, br, kGoldDim);
    }
}

// Ornamento d'angolo: losanga dorata con punto centrale
void DrawCornerGem(Vector2 c) {
    DrawPoly(c, 4, 13.0f, 0.0f, kIron);
    DrawPolyLines(c, 4, 13.0f, 0.0f, kGold);
    DrawCircleV(c, 3.5f, kBlood);
}

} // namespace

DiceRollState::DiceRollState(Game* game)
    : GameState(game)
{
    m_type = GameStateType::DiceRoll;

    m_dice.SetRollProvider([this]() -> core::CombatRoll {
        auto* cs = m_game->GetCombatSystem();
        return cs ? cs->RollD20(0) : core::CombatRoll{};
    });
}

Rectangle DiceRollState::Board() const {
    const float w = 720.0f, h = 450.0f;
    return { (GetScreenWidth() - w) / 2.0f, (GetScreenHeight() - h) / 2.0f + 10.0f, w, h };
}

Rectangle DiceRollState::Felt() const {
    const Rectangle b = Board();
    return { b.x + 30.0f, b.y + 84.0f, b.width - 60.0f, b.height - 84.0f - 34.0f };
}

void DiceRollState::Enter() {
    DiceFont();                         // carica il font dei dadi ora, non a meta' frame
    m_dice.SetBounds(Felt());
    m_dice.Reset();
    m_result = core::CombatRoll{};
    m_showResult = false;
    m_time = 0.0f;
}

void DiceRollState::Update(float deltaTime) {
    m_time += deltaTime;
    m_dice.Update(deltaTime);

    if (m_dice.IsBusy()) m_showResult = false;

    core::CombatRoll r;
    if (m_dice.ConsumeResult(r)) {
        m_result = r;
        m_showResult = true;
    }
}

void DiceRollState::Render() {
    const Font& font = UIFont();
    const int sw = GetScreenWidth(), sh = GetScreenHeight();
    const Rectangle b = Board();
    const Rectangle felt = Felt();
    const float cx = b.x + b.width / 2.0f;

    // il mondo congelato dietro si oscura
    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.75f));

    // --- cornice ---------------------------------------------------------
    DrawSpires(b);
    DrawRectangleRec(b, kIron);
    DrawRectangleLinesEx(b, 3.0f, kGoldDim);
    const Rectangle wood = Inset(b, 10.0f);
    DrawRectangleRec(wood, kWood);
    DrawRectangleLinesEx(wood, 1.0f, Fade(kGoldDim, 0.7f));

    // --- feltro ----------------------------------------------------------
    DrawRectangleRec(felt, kFelt);
    DrawRectangleGradientV((int)felt.x, (int)felt.y, (int)felt.width, (int)(felt.height * 0.35f),
                           Fade(BLACK, 0.45f), Fade(BLACK, 0.0f));
    DrawRectangleGradientV((int)felt.x, (int)(felt.y + felt.height * 0.65f), (int)felt.width,
                           (int)(felt.height * 0.35f), Fade(BLACK, 0.0f), Fade(BLACK, 0.45f));

    // bagliore di candela agli angoli inferiori (con sfarfallio)
    BeginScissorMode((int)felt.x, (int)felt.y, (int)felt.width, (int)felt.height);
    {
        const float f1 = 0.78f + 0.22f * std::sin(m_time * 7.3f) * std::sin(m_time * 2.1f + 1.0f);
        const float f2 = 0.78f + 0.22f * std::sin(m_time * 6.1f + 2.0f) * std::sin(m_time * 3.3f);
        const Color warm = { 255, 150, 60, 255 };
        DrawCircleGradient((int)felt.x + 8, (int)(felt.y + felt.height - 8), 190.0f * f1,
                           Fade(warm, 0.30f * f1), Fade(warm, 0.0f));
        DrawCircleGradient((int)(felt.x + felt.width - 8), (int)(felt.y + felt.height - 8), 190.0f * f2,
                           Fade(warm, 0.30f * f2), Fade(warm, 0.0f));
    }
    EndScissorMode();

    // sigillo: anello runico e pentagramma che ruota lentamente
    {
        const Vector2 c = { felt.x + felt.width / 2.0f, felt.y + felt.height / 2.0f };
        const float r = 140.0f;
        const Color g = Fade(kGold, 0.26f);
        DrawRing(c, r - 3.0f, r, 0.0f, 360.0f, 72, g);
        DrawRing(c, r * 0.80f, r * 0.80f + 1.5f, 0.0f, 360.0f, 72, Fade(kGold, 0.18f));
        for (int i = 0; i < 36; ++i) {
            const float a = i * (2.0f * PI / 36.0f) - m_time * 0.05f;
            const float r2 = r - ((i % 3 == 0) ? 14.0f : 8.0f);
            DrawLineV({ c.x + std::cos(a) * r2, c.y + std::sin(a) * r2 },
                      { c.x + std::cos(a) * (r - 3.0f), c.y + std::sin(a) * (r - 3.0f) }, g);
        }
        Vector2 p[5];
        for (int i = 0; i < 5; ++i) {
            const float a = -PI / 2.0f + i * (2.0f * PI / 5.0f) + m_time * 0.08f;
            p[i] = { c.x + std::cos(a) * r * 0.80f, c.y + std::sin(a) * r * 0.80f };
        }
        for (int i = 0; i < 5; ++i) DrawLineEx(p[i], p[(i + 2) % 5], 1.5f, Fade(kGold, 0.22f));
    }

    DrawRectangleLinesEx(felt, 4.0f, kFeltDark);
    DrawRectangleLinesEx(Inset(felt, 4.0f), 1.0f, Fade(kGoldDim, 0.6f));

    // ornamenti d'angolo
    DrawCornerGem({ b.x + 14.0f, b.y + 14.0f });
    DrawCornerGem({ b.x + b.width - 14.0f, b.y + 14.0f });
    DrawCornerGem({ b.x + 14.0f, b.y + b.height - 14.0f });
    DrawCornerGem({ b.x + b.width - 14.0f, b.y + b.height - 14.0f });

    // --- messaggio (nessun titolo) ---------------------------------------
    const char* banner = "";
    Color bannerColor = kBone;
    if (m_showResult) {
        const char* label = "Tiro normale";
        if (m_result.result == core::CombatResult::CriticalSuccess) {
            label = "Successo critico!"; bannerColor = kGoldBright;
        } else if (m_result.result == core::CombatResult::CriticalFailure) {
            label = "Fallimento critico!"; bannerColor = kBlood;
        }
        banner = TextFormat("%d  -  %s", m_result.rollValue, label);
    } else {
        switch (m_dice.GetState()) {
            case ui::DiceRoller::State::Idle:     banner = "Afferra il dado e scaglialo sul feltro"; break;
            case ui::DiceRoller::State::Dragging: banner = "Lascia andare con uno scatto!";          break;
            default:                              banner = "Il destino rotola...";                   break;
        }
        bannerColor = Fade(kBone, 0.75f);
    }
    DrawTextCentered(font, banner, cx, b.y + 30.0f, 32.0f, bannerColor);

    // --- dado ------------------------------------------------------------
    m_dice.Render();

    // --- pie' di pagina --------------------------------------------------
    DrawTextCentered(font, "INVIO / ESC : chiudi", cx, b.y + b.height + 14.0f, 22.0f, Fade(kBone, 0.7f));
}

void DiceRollState::HandleInput() {
    m_dice.HandleInput();

    if (!m_dice.IsBusy() && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE))) {
        m_game->SetState(std::make_unique<PlayingState>(m_game));
        return;   // questo stato e' stato distrutto: non toccare i membri
    }
}