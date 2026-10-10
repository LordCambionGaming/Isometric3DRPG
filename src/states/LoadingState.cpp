#include "states/LoadingState.h"

#include "core/Game.h"
#include "ui/GothicTheme.h"
#include <raylib.h>

LoadingState::LoadingState(Game* game) : GameState(game) {
    m_type = GameStateType::Loading;
}

void LoadingState::Enter() {
    m_skipFirstUpdate = true;
}

void LoadingState::Exit() {}

void LoadingState::Update(float deltaTime) {
    if (m_skipFirstUpdate) {
        m_skipFirstUpdate = false;
        return;
    }
    if (m_game) m_game->AdvanceSessionLoading(deltaTime);
}

void LoadingState::Render() {
    const Font& font = ui::theme::UIFont();
    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();
    const Color background{ 15, 19, 22, 255 };
    const Color gold{ 205, 168, 92, 255 };
    const Color muted{ 167, 174, 174, 255 };
    const float barWidth = 420.0f;
    const float barHeight = 12.0f;
    const float barX = (screenWidth - barWidth) * 0.5f;
    const float barY = screenHeight * 0.57f;

    ClearBackground(background);
    const char* title = "Preparazione del mondo";
    const Vector2 titleSize = MeasureTextEx(font, title, 30.0f, 1.0f);
    DrawTextEx(font, title, { (screenWidth - titleSize.x) * 0.5f, barY - 92.0f }, 30.0f, 1.0f, gold);

    const std::string& message = m_game->GetLoadingMessage();
    const Vector2 messageSize = MeasureTextEx(font, message.c_str(), 18.0f, 1.0f);
    DrawTextEx(font, message.c_str(), { (screenWidth - messageSize.x) * 0.5f, barY - 42.0f },
               18.0f, 1.0f, muted);

    DrawRectangleRounded({ barX, barY, barWidth, barHeight }, 0.5f, 8, Color{ 45, 50, 51, 255 });
    const float progress = m_game->GetLoadingProgress();
    if (progress > 0.0f) {
        DrawRectangleRounded({ barX, barY, barWidth * progress, barHeight }, 0.5f, 8, gold);
    }
    DrawRectangleRoundedLines({ barX, barY, barWidth, barHeight }, 0.5f, 8, Color{ 98, 91, 75, 255 });

    const char* hint = "Il mondo viene generato in background";
    const Vector2 hintSize = MeasureTextEx(font, hint, 14.0f, 1.0f);
    DrawTextEx(font, hint, { (screenWidth - hintSize.x) * 0.5f, barY + 28.0f }, 14.0f, 1.0f, muted);
}

void LoadingState::HandleInput() {}
