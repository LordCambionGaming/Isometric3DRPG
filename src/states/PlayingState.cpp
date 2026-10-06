#include "states/PlayingState.h"
#include "states/DiceRollState.h"
#include "core/Game.h"
#include "entities/Player.h"
#include "entities/Mob.h"
#include <raylib.h>
#include <raymath.h>
#include <memory>
#include <states/InventoryState.h>

PlayingState::PlayingState(Game* game)
    : GameState(game)
    , m_showDebugInfo(true)
{
    m_type = GameStateType::Playing;
}

void PlayingState::Enter() {}

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
    const char* resultText = "Tiro normale.";
    switch (m_lastRoll.result) {
        case core::CombatResult::CriticalFailure:
            textColor = RED;
            resultText = "CRITICAL FAILURE!";
            break;
        case core::CombatResult::CriticalSuccess:
            textColor = GREEN;
            resultText = "CRITICAL SUCCESS!";
            break;
        default:
            break;
    }

    const char* numText = TextFormat("D20: %d", m_lastRoll.rollValue);
    DrawText(numText, cx - MeasureText(numText, fontSize) / 2,
             cy - fontSize, fontSize, textColor);
    DrawText(resultText, cx - MeasureText(resultText, fontSize) / 2,
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

    // R: Apre il lancio dadi libero (senza bersaglio)
    if (IsKeyPressed(KEY_R)) {
        m_game->SetState(std::make_unique<DiceRollState>(m_game, nullptr));
        return; // PlayingState è stato distrutto da SetState
    }

    Player* player = m_game->GetPlayer();
    if (!player) return;

    // F: Attacca il Mob puntato o selezionato
        if (IsKeyPressed(KEY_F)) {
        Mob* targetMob = m_game->GetHoveredMob();

        if (targetMob && targetMob->IsAttackable()) {
            const float range = player->GetCharacter().ActiveWeapon().range;   // arma equipaggiata, altrimenti pugni
            const float dist = Vector3Distance(player->GetPosition(), targetMob->GetPosition());

            if (dist <= range) {
                m_game->SetState(std::make_unique<DiceRollState>(m_game, targetMob));
                return;
            }
            TraceLog(LOG_INFO, "[COMBAT] Bersaglio troppo lontano! Distanza: %.2fm (Raggio: %.2fm)", dist, range);
        }
    }

    if (IsKeyPressed(KEY_I)) { m_game->SetState(std::make_unique<InventoryState>(m_game)); return; }

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