#include "states/CharacterCreationState.h"
#include "states/MainMenuState.h"
#include "core/Game.h"
#include "entities/Player.h"
#include "ui/GothicTheme.h"
#include <raylib.h>
#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>
#include <states/PlayingState.h>

using namespace ui::theme;

namespace {

constexpr float kPanelW = 540.0f;
constexpr float kPanelH = 560.0f;
constexpr float kRowH = 36.0f;

void DrawLabel(const Font& font, const char* text, float x, float y, float size, Color color) {
    const float spacing = size / 10.0f;
    DrawTextEx(font, text, { x + 2.0f, y + 2.0f }, size, spacing, Fade(BLACK, 0.8f));
    DrawTextEx(font, text, { x, y }, size, spacing, color);
}

void DrawRowHighlight(const Rectangle& r) {
    DrawRectangleRec(r, Fade(kGold, 0.14f));
    DrawRectangleLinesEx(r, 1.0f, Fade(kGold, 0.7f));
}

} // namespace

CharacterCreationState::CharacterCreationState(Game* game)
    : GameState(game)
{
    m_type = GameStateType::CharacterCreation;
    m_assigned.fill(-1);
}

void CharacterCreationState::Enter() {
    // I sei tiri avvengono qui, una sola volta per ingresso nella Character Creation.
    m_pool = core::RollAbilityPool();
    m_assigned.fill(-1);
    m_class = core::CharacterClass::Warrior;
    m_row = kRowClass;

    std::cout << "[CHARACTER] 4d6 drop lowest x6 ->";
    for (int score : m_pool) std::cout << ' ' << score;
    std::cout << std::endl;
}

void CharacterCreationState::Update(float) {}

bool CharacterCreationState::AllAssigned() const {
    return std::all_of(m_assigned.begin(), m_assigned.end(), [](int s) { return s >= 0; });
}

bool CharacterCreationState::IsPoolSlotUsed(int slot) const {
    return std::find(m_assigned.begin(), m_assigned.end(), slot) != m_assigned.end();
}

void CharacterCreationState::CycleClass(int dir) {
    const int n = core::kCharacterClassCount;
    m_class = static_cast<core::CharacterClass>((static_cast<int>(m_class) + dir + n) % n);
}

void CharacterCreationState::CycleAssignment(int ability, int dir) {
    // Opzioni: "non assegnato" (-1) + ogni punteggio del pool non gia' preso da un'altra caratteristica.
    std::vector<int> options{ -1 };
    for (int slot = 0; slot < core::kAbilityCount; ++slot) {
        bool takenByOther = false;
        for (int a = 0; a < core::kAbilityCount; ++a) {
            if (a != ability && m_assigned[static_cast<size_t>(a)] == slot) takenByOther = true;
        }
        if (!takenByOther) options.push_back(slot);
    }
    const auto it = std::find(options.begin(), options.end(), m_assigned[static_cast<size_t>(ability)]);
    const int count = static_cast<int>(options.size());
    int pos = (it == options.end()) ? 0 : static_cast<int>(it - options.begin());
    pos = (pos + dir + count) % count;
    m_assigned[static_cast<size_t>(ability)] = options[static_cast<size_t>(pos)];
}

core::Character CharacterCreationState::Build() const {
    core::AbilityScores scores{};
    for (int a = 0; a < core::kAbilityCount; ++a) {
        const int slot = m_assigned[static_cast<size_t>(a)];
        scores[static_cast<size_t>(a)] = (slot >= 0) ? m_pool[static_cast<size_t>(slot)] : 0;
    }
    return core::CreateCharacter(m_class, scores);
}

void CharacterCreationState::HandleInput() {
    if (IsKeyPressed(KEY_ESCAPE)) {
        std::cout << "STATE CHANGE: CharacterCreationState -> MainMenuState" << std::endl;
        if (m_game) m_game->SetState(std::make_unique<MainMenuState>(m_game));
        return;   // questo stato e' stato distrutto: non toccare i membri
    }

    if (IsKeyPressed(KEY_DOWN)) m_row = (m_row + 1) % kRowCount;
    if (IsKeyPressed(KEY_UP))   m_row = (m_row + kRowCount - 1) % kRowCount;

    const int dir = (IsKeyPressed(KEY_RIGHT) ? 1 : 0) - (IsKeyPressed(KEY_LEFT) ? 1 : 0);
    if (dir != 0) {
        if (m_row == kRowClass) {
            CycleClass(dir);
        } else if (m_row >= kRowFirstAbility && m_row < kRowConfirm) {
            CycleAssignment(m_row - kRowFirstAbility, dir);
        }
    }

    if (IsKeyPressed(KEY_ENTER) && m_row == kRowConfirm && AllAssigned()) {
        const core::Character character = Build();   // prima di SetState: lo stato viene distrutto
        std::cout << "STATE CHANGE: CharacterCreationState -> PlayingState (" << core::ToString(character.characterClass)
                  << ", STR " << character.Score(core::Ability::Str)
                  << " DEX " << character.Score(core::Ability::Dex)
                  << " CON " << character.Score(core::Ability::Con)
                  << " INT " << character.Score(core::Ability::Int)
                  << " WIS " << character.Score(core::Ability::Wis)
                  << " CHA " << character.Score(core::Ability::Cha) 
                  << ", HP " << character.vitals.currentHP << "/" << character.MaxHP()<<")" << std::endl;
        if (m_game) {
            // Stessa sequenza che prima faceva "Nuova partita" nel menu, ora dopo la conferma.
            m_game->ResetGameSession();
            m_game->CreateGameSession();
            if (Player* player = m_game->GetPlayer()) player->SetCharacter(character);
            m_game->SetState(std::make_unique<PlayingState>(m_game));
        }
        return;   // questo stato e' stato distrutto: non toccare i membri
    }
}

void CharacterCreationState::Render() {
    const Font& font = UIFont();
    const int sw = GetScreenWidth(), sh = GetScreenHeight();
    const Rectangle b = { (sw - kPanelW) / 2.0f, (sh - kPanelH) / 2.0f, kPanelW, kPanelH };
    const float cx = b.x + b.width / 2.0f;
    const float left = b.x + 36.0f;
    const float rowW = b.width - 72.0f;

    // Il mondo non e' visibile in questo stato: sfondo pieno e pannello gotico
    DrawRectangle(0, 0, sw, sh, kIron);
    DrawRectangleRec(b, kWood);
    DrawRectangleLinesEx(b, 3.0f, kGoldDim);
    DrawRectangleLinesEx({ b.x + 8.0f, b.y + 8.0f, b.width - 16.0f, b.height - 16.0f }, 1.0f, Fade(kGoldDim, 0.7f));

    static const core::Character kSheet{};   // valori fissi: Human, livello 1, XP 0
    DrawTextCentered(font, "Creazione del personaggio", cx, b.y + 22.0f, 30.0f, kGoldBright);
    DrawTextCentered(font, TextFormat("%s  -  Livello %d  -  XP %d", core::ToString(kSheet.race), kSheet.level, kSheet.xp),
                     cx, b.y + 62.0f, 20.0f, Fade(kBone, 0.8f));

    auto rowColor = [&](int row) { return (m_row == row) ? kGoldBright : kBone; };

    // --- classe ----------------------------------------------------------
    float y = b.y + 104.0f;
    if (m_row == kRowClass) DrawRowHighlight({ left - 10.0f, y - 4.0f, rowW + 20.0f, kRowH - 2.0f });
    DrawLabel(font, "Classe", left, y, 24.0f, rowColor(kRowClass));
    DrawLabel(font, TextFormat("<  %s  >", core::ToString(m_class)), left + 170.0f, y, 24.0f, rowColor(kRowClass));

    // --- pool dei punteggi tirati ---------------------------------------
    y += kRowH + 10.0f;
    DrawLabel(font, "Tiri (4d6, scarta il piu' basso)", left, y, 20.0f, Fade(kBone, 0.75f));
    y += 28.0f;
    for (int i = 0; i < core::kAbilityCount; ++i) {
        const Rectangle slot = { left + 8.0f + i * 80.0f, y, 64.0f, 34.0f };
        const bool used = IsPoolSlotUsed(i);
        DrawRectangleRec(slot, Fade(BLACK, used ? 0.25f : 0.45f));
        DrawRectangleLinesEx(slot, 1.0f, Fade(kGoldDim, used ? 0.35f : 0.9f));
        const char* t = TextFormat("%d", m_pool[static_cast<size_t>(i)]);
        const Vector2 m = MeasureTextEx(font, t, 24.0f, 2.4f);
        DrawLabel(font, t, slot.x + (slot.width - m.x) / 2.0f, slot.y + 5.0f, 24.0f,
                  used ? Fade(kBone, 0.3f) : kGoldBright);
    }

    // --- caratteristiche -------------------------------------------------
    y += 34.0f + 18.0f;
    for (int a = 0; a < core::kAbilityCount; ++a) {
        const int row = kRowFirstAbility + a;
        if (m_row == row) DrawRowHighlight({ left - 10.0f, y - 4.0f, rowW + 20.0f, kRowH - 2.0f });
        const int slot = m_assigned[static_cast<size_t>(a)];
        DrawLabel(font, core::ToString(static_cast<core::Ability>(a)), left, y, 24.0f, rowColor(row));
        if (slot >= 0) {
            const int score = m_pool[static_cast<size_t>(slot)];
            DrawLabel(font, TextFormat("<  %d  >", score), left + 170.0f, y, 24.0f, rowColor(row));
            DrawLabel(font, TextFormat("%+d", core::AbilityModifier(score)), left + 340.0f, y, 24.0f, rowColor(row));
        } else {
            DrawLabel(font, "<  --  >", left + 170.0f, y, 24.0f, Fade(rowColor(row), 0.6f));
        }
        y += kRowH;
    }

    // --- arma iniziale ---------------------------------------------------
    y += 8.0f;
    const core::WeaponDef weapon = core::StartingWeapon(m_class);
    DrawLabel(font, TextFormat("Arma iniziale: %s %s", weapon.name, core::ToString(weapon.damage).c_str()),
              left, y, 20.0f, Fade(kBone, 0.85f));
    DrawLabel(font, "Posseduta, non equipaggiata", left, y + 24.0f, 18.0f, Fade(kBone, 0.6f));

    // --- conferma --------------------------------------------------------
    y += 62.0f;
    const bool ready = AllAssigned();
    if (m_row == kRowConfirm) DrawRowHighlight({ left - 10.0f, y - 4.0f, rowW + 20.0f, kRowH - 2.0f });
    DrawTextCentered(font, ready ? "[ Conferma ]" : "[ Conferma ]  assegna tutti i punteggi", cx, y, 24.0f,
                     ready ? rowColor(kRowConfirm) : Fade(kBone, 0.4f));

    DrawTextCentered(font, "SU/GIU: riga   SX/DX: cambia   INVIO: conferma   ESC: indietro", cx,
                     b.y + b.height + 14.0f, 20.0f, Fade(kBone, 0.7f));
}