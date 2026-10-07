#include "states/CharacterCreationState.h"
#include "core/KeyBindings.h"
#include "core/Localization.h"
#include "states/MainMenuState.h"
#include "states/DiceRollState.h"
#include "core/Game.h"
#include "entities/Player.h"
#include "ui/GothicTheme.h"
#include <raylib.h>
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <states/PlayingState.h>

using namespace ui::theme;

namespace {

constexpr float kPanelW = 540.0f;
constexpr float kPanelH = 640.0f;
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

std::string MethodName(CharacterCreationState::Method method) {
    return lang::Get(method == CharacterCreationState::Method::Roll
        ? "character.method_roll" : "character.method_fixed");
}

} // namespace

CharacterCreationState::CharacterCreationState(Game* game)
    : GameState(game)
{
    m_type = GameStateType::CharacterCreation;
    m_assigned.fill(-1);
    m_rolled.fill(false);
}

CharacterCreationState::CharacterCreationState(Game* game, const Progress& progress)
    : GameState(game)
    , m_method(progress.method)
    , m_class(progress.characterClass)
    , m_pool(progress.pool)
    , m_rolled(progress.rolled)
    , m_assigned(progress.assigned)
    , m_row(progress.row)
    , m_restored(true)
{
    m_type = GameStateType::CharacterCreation;
}

CharacterCreationState::Progress CharacterCreationState::Snapshot() const {
    Progress p;
    p.method = m_method;
    p.characterClass = m_class;
    p.pool = m_pool;
    p.rolled = m_rolled;
    p.assigned = m_assigned;
    p.row = m_row;
    return p;
}

void CharacterCreationState::Enter() {
    if (m_restored) return;   // si torna dalla board dei dadi: la creazione continua da dove era

    // Si parte con "Tira i dadi": nessun punteggio ancora lanciato.
    m_method = Method::Roll;
    m_class = core::CharacterClass::Warrior;
    m_pool.fill(0);
    m_rolled.fill(false);
    m_assigned.fill(-1);
    m_row = kRowRoll;
}

void CharacterCreationState::Update(float) {}

int CharacterCreationState::RolledCount() const {
    return static_cast<int>(std::count(m_rolled.begin(), m_rolled.end(), true));
}

bool CharacterCreationState::RollRowActive() const {
    return m_method == Method::Roll && RolledCount() < core::kAbilityCount;
}

bool CharacterCreationState::AllAssigned() const {
    return std::all_of(m_assigned.begin(), m_assigned.end(), [](int s) { return s >= 0; });
}

bool CharacterCreationState::IsPoolSlotUsed(int slot) const {
    return std::find(m_assigned.begin(), m_assigned.end(), slot) != m_assigned.end();
}

int CharacterCreationState::NextRow(int dir) const {
    int row = m_row;
    for (int i = 0; i < kRowCount; ++i) {
        row = (row + dir + kRowCount) % kRowCount;
        if (row == kRowRoll && !RollRowActive()) continue;   // niente da lanciare: la riga non si seleziona
        return row;
    }
    return m_row;
}

void CharacterCreationState::SetMethod(Method method) {
    m_method = method;
    m_assigned.fill(-1);                       // i punteggi cambiano: le assegnazioni non valgono piu'
    if (method == Method::Fixed) {
        m_pool = core::StandardArray();
        m_rolled.fill(true);
    } else {
        m_pool.fill(0);
        m_rolled.fill(false);
    }
    if (m_row == kRowRoll && !RollRowActive()) m_row = kRowFirstAbility;
}

void CharacterCreationState::AutoRollRemaining() {
    if (m_method != Method::Roll) return;
    for (int i = 0; i < core::kAbilityCount; ++i) {
        if (!m_rolled[static_cast<size_t>(i)]) {
            m_pool[static_cast<size_t>(i)] = core::RollAbilityScore();
            m_rolled[static_cast<size_t>(i)] = true;
        }
    }
    std::cout << "[CHARACTER] 4d6 drop lowest (auto) ->";
    for (int score : m_pool) std::cout << ' ' << score;
    std::cout << std::endl;
    if (m_row == kRowRoll) m_row = kRowFirstAbility;
}

// Apre la board gotica per il prossimo punteggio da lanciare: 4d6, un dado per volta, poi si scarta il piu' basso.
// La board sostituisce questo stato (SetState lo distrugge): per tornare qui se ne ricrea uno con il punteggio dentro.
void CharacterCreationState::StartBoardRoll() {
    if (!m_game) return;
    int slot = -1;
    for (int i = 0; i < core::kAbilityCount; ++i) {
        if (!m_rolled[static_cast<size_t>(i)]) { slot = i; break; }
    }
    if (slot < 0) return;

    Game* game = m_game;
    Progress snap = Snapshot();

    DiceRollOptions options;
    options.opaqueBackdrop = true;     // dietro la creazione non c'e' il mondo
    options.summary = [](const std::vector<int>& dice) {
        return TextFormat(lang::Get("character.rolled_score").c_str(), core::AbilityScoreFromDice(dice));
    };
    options.onFinish = [game, snap, slot](const std::vector<int>& dice, bool completed) mutable -> std::unique_ptr<GameState> {
        if (completed && dice.size() == 4) {
            snap.pool[static_cast<size_t>(slot)] = core::AbilityScoreFromDice(dice);
            snap.rolled[static_cast<size_t>(slot)] = true;
            std::cout << "[CHARACTER] 4d6 drop lowest (board) slot " << slot << " ->";
            for (int d : dice) std::cout << ' ' << d;
            std::cout << " = " << snap.pool[static_cast<size_t>(slot)] << std::endl;
        }
        const int rolledNow = static_cast<int>(std::count(snap.rolled.begin(), snap.rolled.end(), true));
        snap.row = (rolledNow < core::kAbilityCount) ? kRowRoll : kRowFirstAbility;
        return std::make_unique<CharacterCreationState>(game, snap);
    };

    m_game->SetState(std::make_unique<DiceRollState>(m_game, core::DiceExpr{ 4, 6 }, std::move(options)));
}

void CharacterCreationState::CycleClass(int dir) {
    const int n = core::kCharacterClassCount;
    m_class = static_cast<core::CharacterClass>((static_cast<int>(m_class) + dir + n) % n);
}

void CharacterCreationState::CycleAssignment(int ability, int dir) {
    // Opzioni: "non assegnato" (-1) + ogni punteggio GIA' NOTO del pool non gia' preso da un'altra caratteristica.
    std::vector<int> options{ -1 };
    for (int slot = 0; slot < core::kAbilityCount; ++slot) {
        if (!m_rolled[static_cast<size_t>(slot)]) continue;
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
    if (core::KeyBindings::Pressed(core::KeyBindings::Action::Back)) {
        std::cout << "STATE CHANGE: CharacterCreationState -> MainMenuState" << std::endl;
        if (m_game) m_game->SetState(std::make_unique<MainMenuState>(m_game));
        return;   // questo stato e' stato distrutto: non toccare i membri
    }

    if (core::KeyBindings::Pressed(core::KeyBindings::Action::Down)) m_row = NextRow(+1);
    if (core::KeyBindings::Pressed(core::KeyBindings::Action::Up))   m_row = NextRow(-1);

    if (IsKeyPressed(KEY_A)) AutoRollRemaining();

    const int dir = (core::KeyBindings::Pressed(core::KeyBindings::Action::Right) ? 1 : 0) -
                    (core::KeyBindings::Pressed(core::KeyBindings::Action::Left) ? 1 : 0);
    if (dir != 0) {
        if (m_row == kRowMethod) {
            SetMethod(m_method == Method::Roll ? Method::Fixed : Method::Roll);
        } else if (m_row == kRowClass) {
            CycleClass(dir);
        } else if (m_row >= kRowFirstAbility && m_row < kRowConfirm) {
            CycleAssignment(m_row - kRowFirstAbility, dir);
        }
    }

    if (core::KeyBindings::Pressed(core::KeyBindings::Action::Confirm) && m_row == kRowRoll && RollRowActive()) {
        StartBoardRoll();
        return;   // questo stato e' stato distrutto: non toccare i membri
    }

    if (core::KeyBindings::Pressed(core::KeyBindings::Action::Confirm) && m_row == kRowConfirm && AllAssigned()) {
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
    const std::string raceName = lang::RaceName(core::ToString(kSheet.race));
    DrawTextCentered(font, lang::Get("character.title").c_str(), cx, b.y + 22.0f, 30.0f, kGoldBright);
    DrawTextCentered(font, TextFormat(lang::Get("character.sheet").c_str(), raceName.c_str(), kSheet.level, kSheet.xp),
                     cx, b.y + 62.0f, 20.0f, Fade(kBone, 0.8f));

    auto rowColor = [&](int row) { return (m_row == row) ? kGoldBright : kBone; };

    // --- metodo ----------------------------------------------------------
    float y = b.y + 104.0f;
    if (m_row == kRowMethod) DrawRowHighlight({ left - 10.0f, y - 4.0f, rowW + 20.0f, kRowH - 2.0f });
    DrawLabel(font, lang::Get("character.scores").c_str(), left, y, 24.0f, rowColor(kRowMethod));
    const std::string methodName = MethodName(m_method);
    DrawLabel(font, TextFormat("<  %s  >", methodName.c_str()), left + 170.0f, y, 24.0f, rowColor(kRowMethod));

    // --- classe ----------------------------------------------------------
    y += kRowH;
    if (m_row == kRowClass) DrawRowHighlight({ left - 10.0f, y - 4.0f, rowW + 20.0f, kRowH - 2.0f });
    DrawLabel(font, lang::Get("character.class").c_str(), left, y, 24.0f, rowColor(kRowClass));
    const std::string className = lang::CharacterClassName(core::ToString(m_class));
    DrawLabel(font, TextFormat("<  %s  >", className.c_str()), left + 170.0f, y, 24.0f, rowColor(kRowClass));

    // --- pool dei punteggi ----------------------------------------------
    y += kRowH + 10.0f;
    DrawLabel(font, lang::Get(m_method == Method::Roll ? "character.rolls_label" : "character.array_label").c_str(),
              left, y, 20.0f, Fade(kBone, 0.75f));
    y += 28.0f;
    for (int i = 0; i < core::kAbilityCount; ++i) {
        const Rectangle slot = { left + 8.0f + i * 80.0f, y, 64.0f, 34.0f };
        const bool known = m_rolled[static_cast<size_t>(i)];
        const bool used = known && IsPoolSlotUsed(i);
        DrawRectangleRec(slot, Fade(BLACK, used ? 0.25f : 0.45f));
        DrawRectangleLinesEx(slot, 1.0f, Fade(kGoldDim, (used || !known) ? 0.35f : 0.9f));
        const char* t = known ? TextFormat("%d", m_pool[static_cast<size_t>(i)]) : "?";
        const Vector2 m = MeasureTextEx(font, t, 24.0f, 2.4f);
        DrawLabel(font, t, slot.x + (slot.width - m.x) / 2.0f, slot.y + 5.0f, 24.0f,
                  !known ? Fade(kBone, 0.3f) : (used ? Fade(kBone, 0.3f) : kGoldBright));
    }

    // --- lancio dei dadi -------------------------------------------------
    y += 34.0f + 10.0f;
    if (m_method == Method::Roll) {
        if (RollRowActive()) {
            if (m_row == kRowRoll) DrawRowHighlight({ left - 10.0f, y - 4.0f, rowW + 20.0f, kRowH - 2.0f });
            DrawLabel(font, TextFormat(lang::Get("character.roll_action").c_str(), RolledCount(), core::kAbilityCount), left, y, 22.0f, rowColor(kRowRoll));
            DrawLabel(font, lang::Get("character.roll_help").c_str(), left + 250.0f, y + 3.0f, 18.0f, Fade(kBone, 0.65f));
        } else {
            DrawLabel(font, lang::Get("character.all_rolled").c_str(), left, y, 20.0f, Fade(kBone, 0.6f));
        }
    }
    y += kRowH;

    // --- caratteristiche -------------------------------------------------
    y += 6.0f;
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
    const core::ItemId weaponId = core::StartingWeaponId(m_class);
    const core::WeaponDef weapon = core::ResolveWeapon(weaponId);
    const core::ItemDef* weaponDef = core::GetItem(weaponId);
    const std::string weaponName = weaponDef ? lang::ItemName(weaponDef->key) : weapon.name;
    DrawLabel(font, TextFormat(lang::Get("character.initial_weapon").c_str(), weaponName.c_str(), core::ToString(weapon.damage).c_str()),
              left, y, 20.0f, Fade(kBone, 0.85f));
    DrawLabel(font, lang::Get("character.owned_not_equipped").c_str(), left, y + 24.0f, 18.0f, Fade(kBone, 0.6f));

    // --- conferma --------------------------------------------------------
    y += 62.0f;
    const bool ready = AllAssigned();
    if (m_row == kRowConfirm) DrawRowHighlight({ left - 10.0f, y - 4.0f, rowW + 20.0f, kRowH - 2.0f });
    DrawTextCentered(font, lang::Get(ready ? "character.confirm" : "character.confirm_assign").c_str(), cx, y, 24.0f,
                     ready ? rowColor(kRowConfirm) : Fade(kBone, 0.4f));

    DrawTextCentered(font, lang::Get("character.help").c_str(), cx,
                     b.y + b.height + 14.0f, 20.0f, Fade(kBone, 0.7f));
}