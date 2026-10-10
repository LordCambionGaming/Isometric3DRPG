#include "states/PlayingState.h"
#include "states/DiceRollState.h"
#include "core/Game.h"
#include "core/KeyBindings.h"
#include "core/Localization.h"
#include "entities/Player.h"
#include "entities/Mob.h"
#include <raylib.h>
#include <raymath.h>
#include <memory>
#include "audio/AudioManager.h"
#include <states/InventoryState.h>

PlayingState::PlayingState(Game* game)
    : GameState(game)
    , m_showDebugInfo(true)
{
    m_type = GameStateType::Playing;
}

void PlayingState::Enter() {
    if (AudioManager* audio = m_game->GetAudioManager()) {
        audio->SetPlaylist({ "music/adventureTogether.mp3", "music/luter.mp3" }, true);
    }
}

void PlayingState::Exit() {}

void PlayingState::Update(float deltaTime) {
    // Il testo del tiro di debug/overlay scompare dopo 2 secondi
    if (m_lastRoll.rollValue != 0) {
        m_rollTimer += deltaTime;
        if (m_rollTimer >= 2.0f) {
            m_lastRoll = core::CombatRoll{};
            m_rollTimer = 0.0f;
        }
    }
}

void PlayingState::Render() {
    if (m_lastRoll.rollValue == 0) return;

    const int fontSize = 48;
    const int cx = GetScreenWidth() / 2;
    const int cy = GetScreenHeight() / 2;

    Color textColor = WHITE;
    std::string resultText = lang::Get("dice.normal");
    switch (m_lastRoll.result) {
        case core::CombatResult::CriticalFailure:
            textColor = RED;
            resultText = lang::Get("dice.critical_failure");
            break;
        case core::CombatResult::CriticalSuccess:
            textColor = GREEN;
            resultText = lang::Get("dice.critical_success");
            break;
        default:
            break;
    }

    const char* numText = TextFormat(lang::Get("playing.d20").c_str(), m_lastRoll.rollValue);
    DrawText(numText, cx - MeasureText(numText, fontSize) / 2,
             cy - fontSize, fontSize, textColor);
    DrawText(resultText.c_str(), cx - MeasureText(resultText.c_str(), fontSize) / 2,
             cy + 10, fontSize, textColor);
}

void PlayingState::HandleInput() {
    // F3: Toggle debug info
    if (IsKeyPressed(KEY_F3)) {
        m_showDebugInfo = !m_showDebugInfo;
    }

    // F5: Tiro D20 istantaneo di test
    if (IsKeyPressed(KEY_F5) && m_game->GetCombatSystem()) {
        m_lastRoll = m_game->GetCombatSystem()->TestCombatRoll(0);
        m_rollTimer = 0.0f;
    }


    Player* player = m_game->GetPlayer();
    if (!player) return;



    if (core::KeyBindings::Pressed(core::KeyBindings::Action::Inventory)) {
        m_game->SetState(std::make_unique<InventoryState>(m_game));
        return;
    }

    // F6 (debug): aggiunge un esemplare di ogni arma del catalogo, per provare le animazioni 1H / 2H / arco
    if (IsKeyPressed(KEY_F6)) {
        core::Character& c = player->GetCharacterMutable();
        for (const core::ItemDef& d : core::AllItems()) {
            if (d.category == core::ItemCategory::Weapon) c.AddItem(d.id);
        }
    }

    // Tasti rapidi di test vita/danno giocatore
    if (IsKeyPressed(KEY_H)) player->TakeDamage(1);
    if (IsKeyPressed(KEY_J)) player->TakeDamage(5);
    if (IsKeyPressed(KEY_K)) player->Heal(player->GetCharacter().MaxHP());
}
