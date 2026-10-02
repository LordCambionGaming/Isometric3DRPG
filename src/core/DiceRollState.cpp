#include "core/DiceRollState.h"
#include "core/Game.h"
#include <raylib.h>

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
    const float w = 640.0f, h = 420.0f;
    return { (GetScreenWidth() - w) / 2.0f, (GetScreenHeight() - h) / 2.0f, w, h };
}

void DiceRollState::Enter() {
    const Rectangle b = Board();
    // area di gioco del dado: dentro il pannello, sotto il titolo
    m_dice.SetBounds({ b.x + 20, b.y + 60, b.width - 40, b.height - 80 });
    m_dice.Reset();
    m_result = core::CombatRoll{};
}

void DiceRollState::Update(float deltaTime) {
    m_dice.Update(deltaTime);

    core::CombatRoll r;
    if (m_dice.ConsumeResult(r)) {
        m_result = r;
    }
}

void DiceRollState::Render() {
    // oscura il mondo congelato dietro
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.6f));

    // board
    const Rectangle b = Board();
    DrawRectangleRounded(b, 0.05f, 8, { 60, 40, 25, 255 });
    DrawRectangleRounded({ b.x + 12, b.y + 12, b.width - 24, b.height - 24 }, 0.04f, 8, { 30, 90, 55, 255 });
    DrawRectangleRoundedLines(b, 0.05f, 8, { 120, 85, 50, 255 });

    const char* title = "TIRO DI D20";
    DrawText(title, (int)(b.x + b.width / 2) - MeasureText(title, 28) / 2, (int)b.y + 22, 28, RAYWHITE);

    // aiuto
    if (m_dice.GetState() == ui::DiceRoller::State::Idle) {
        const char* hint = "Trascina il dado e rilascialo mentre muovi il mouse";
        DrawText(hint, (int)(b.x + b.width / 2) - MeasureText(hint, 18) / 2,
                 (int)(b.y + b.height / 2) - 9, 18, Fade(WHITE, 0.7f));
    }

    m_dice.Render();

    // risultato a dado fermo
    if (m_dice.GetState() == ui::DiceRoller::State::Settled) {
        Color c = WHITE;
        const char* label = "Tiro normale";
        if (m_result.result == core::CombatResult::CriticalSuccess) { c = GREEN; label = "CRITICAL SUCCESS!"; }
        else if (m_result.result == core::CombatResult::CriticalFailure) { c = RED; label = "CRITICAL FAILURE!"; }

        const char* num = TextFormat("%d", m_result.rollValue);
        DrawText(num, (int)(b.x + b.width / 2) - MeasureText(num, 72) / 2, (int)b.y + 90, 72, c);
        DrawText(label, (int)(b.x + b.width / 2) - MeasureText(label, 28) / 2, (int)b.y + 170, 28, c);
    }

    const char* foot = "INVIO / ESC: torna al gioco";
    DrawText(foot, (int)(b.x + b.width / 2) - MeasureText(foot, 16) / 2,
             (int)(b.y + b.height) + 12, 16, Fade(WHITE, 0.8f));
}

void DiceRollState::HandleInput() {
    m_dice.HandleInput();

    if (!m_dice.IsBusy() && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE))) {
        m_game->SetState(std::make_unique<PlayingState>(m_game));
        return;   // questo stato è stato distrutto: non toccare i membri
    }
}