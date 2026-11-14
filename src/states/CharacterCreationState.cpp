#include "states/CharacterCreationState.h"
#include "assets/AssetManager.h"
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

using namespace ui::theme;

namespace {

struct HairStyleDef {
    const char* id;
    const char* label;
};

const HairStyleDef kMaleHairStyles[] = {
    { "crop", "Crop" },
    { "side_part", "Side part" },
    { "medium", "Medium" },
    { "tied_back", "Tied back" }
};
constexpr int kMaleHairStyleCount = 4;

const HairStyleDef kFemaleHairStyles[] = {
    { "ponytail", "Ponytail" },
    { "bob", "Bob" },
    { "long", "Long" },
    { "braid", "Braid" }
};
constexpr int kFemaleHairStyleCount = 4;

struct PaletteColorDef {
    const char* name;
    Color color;
};

const PaletteColorDef kSkinPalette[] = {
    { "Porcelain",   { (unsigned char)(0.86f * 255), (unsigned char)(0.70f * 255), (unsigned char)(0.60f * 255), 255 } },
    { "Fair Warm",   { (unsigned char)(0.77f * 255), (unsigned char)(0.515f * 255), (unsigned char)(0.365f * 255), 255 } },
    { "Tan",         { (unsigned char)(0.66f * 255), (unsigned char)(0.43f * 255), (unsigned char)(0.30f * 255), 255 } },
    { "Warm Medium", { (unsigned char)(0.57f * 255), (unsigned char)(0.335f * 255), (unsigned char)(0.225f * 255), 255 } },
    { "Olive",       { (unsigned char)(0.47f * 255), (unsigned char)(0.31f * 255), (unsigned char)(0.20f * 255), 255 } },
    { "Brown",       { (unsigned char)(0.38f * 255), (unsigned char)(0.23f * 255), (unsigned char)(0.16f * 255), 255 } },
    { "Deep Brown",  { (unsigned char)(0.30f * 255), (unsigned char)(0.18f * 255), (unsigned char)(0.13f * 255), 255 } },
    { "Dark",        { (unsigned char)(0.19f * 255), (unsigned char)(0.105f * 255), (unsigned char)(0.075f * 255), 255 } }
};
constexpr int kSkinPaletteCount = 8;

const PaletteColorDef kHairPalette[] = {
    { "Black",       { (unsigned char)(0.025f * 255), (unsigned char)(0.018f * 255), (unsigned char)(0.016f * 255), 255 } },
    { "Dark Brown",  { (unsigned char)(0.10f * 255),  (unsigned char)(0.040f * 255), (unsigned char)(0.020f * 255), 255 } },
    { "Chestnut",    { (unsigned char)(0.30f * 255),  (unsigned char)(0.085f * 255), (unsigned char)(0.030f * 255), 255 } },
    { "Dark Blonde", { (unsigned char)(0.50f * 255),  (unsigned char)(0.320f * 255), (unsigned char)(0.110f * 255), 255 } },
    { "Blonde",      { (unsigned char)(0.72f * 255),  (unsigned char)(0.460f * 255), (unsigned char)(0.130f * 255), 255 } },
    { "Ginger",      { (unsigned char)(0.55f * 255),  (unsigned char)(0.160f * 255), (unsigned char)(0.050f * 255), 255 } },
    { "Auburn",      { (unsigned char)(0.48f * 255),  (unsigned char)(0.075f * 255), (unsigned char)(0.025f * 255), 255 } },
    { "Silver",      { (unsigned char)(0.48f * 255),  (unsigned char)(0.500f * 255), (unsigned char)(0.550f * 255), 255 } },
    { "White",       { (unsigned char)(0.86f * 255),  (unsigned char)(0.860f * 255), (unsigned char)(0.880f * 255), 255 } },
    { "Platinum",    { (unsigned char)(0.84f * 255),  (unsigned char)(0.770f * 255), (unsigned char)(0.610f * 255), 255 } }
};
constexpr int kHairPaletteCount = 10;

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
    , m_page(progress.page)
    , m_row(progress.row)
    , m_appearance(progress.appearance)
    , m_method(progress.method)
    , m_class(progress.characterClass)
    , m_pool(progress.pool)
    , m_rolled(progress.rolled)
    , m_assigned(progress.assigned)
    , m_restored(true)
{
    m_type = GameStateType::CharacterCreation;
}

CharacterCreationState::Progress CharacterCreationState::Snapshot() const {
    Progress p;
    p.page = m_page;
    p.row = m_row;
    p.appearance = m_appearance;
    p.method = m_method;
    p.characterClass = m_class;
    p.pool = m_pool;
    p.rolled = m_rolled;
    p.assigned = m_assigned;
    return p;
}

void CharacterCreationState::Enter() {
    // Inizializza camera 3D per il preview
    m_previewCamera.position = Vector3{ 0.0f, 1.25f, 3.2f };
    m_previewCamera.target = Vector3{ 0.0f, 0.95f, 0.0f };
    m_previewCamera.up = Vector3{ 0.0f, 1.0f, 0.0f };
    m_previewCamera.fovy = 38.0f;
    m_previewCamera.projection = CAMERA_PERSPECTIVE;
    m_previewRotation = 0.0f;
    m_previewAnimTime = 0.0f;

    if (m_restored) {
        UpdatePreviewVisuals();
        return;
    }

    m_page = kPageAppearance;
    m_row = 0;
    m_appearance.gender = core::Gender::Male;
    m_appearance.name = "Cambion";
    m_appearance.hairStyle = "crop";
    m_appearance.skinColorIndex = 1;
    m_appearance.hairColorIndex = 0;

    m_method = Method::Roll;
    m_class = core::CharacterClass::Warrior;
    m_pool.fill(0);
    m_rolled.fill(false);
    m_assigned.fill(-1);

    UpdatePreviewVisuals();
}

void CharacterCreationState::UpdatePreviewVisuals() {
    if (!m_game) return;
    AssetManager* assets = m_game->GetAssetManager();
    if (!assets) return;

    const bool isFemale = (m_appearance.gender == core::Gender::Female);
    const std::string bodyPath = isFemale
        ? "models/characters/bases/human_female_base.glb"
        : "models/characters/bases/human_male_base.glb";

    if (m_lastLoadedGender != m_appearance.gender || !m_previewBodyModel) {
        m_previewBodyModel = assets->LoadModel(bodyPath);
        m_previewAnims = assets->LoadAnimations(bodyPath, m_previewAnimCount);
        m_previewIdleIndex = -1;
        if (m_previewAnims && m_previewAnimCount > 0) {
            for (int i = 0; i < m_previewAnimCount; ++i) {
                const std::string aName = m_previewAnims[i].name ? m_previewAnims[i].name : "";
                if (aName.find("Idle") != std::string::npos) {
                    m_previewIdleIndex = i;
                    break;
                }
            }
            if (m_previewIdleIndex < 0 && m_previewAnimCount > 0) m_previewIdleIndex = 0;
        }
        m_lastLoadedGender = m_appearance.gender;
    }

    // Palette pelle sul corpo
    // Raylib carica i materiali GLTF con offset +1: materials[0]=default, materials[1]=Areola, materials[2]=Skin, etc.
    if (m_previewBodyModel) {
        const int sIdx = std::clamp(m_appearance.skinColorIndex, 0, kSkinPaletteCount - 1);
        const Color sCol = kSkinPalette[sIdx].color;
        // materials[2] = Skin (GLTF material[1])
        if (m_previewBodyModel->materialCount > 2) {
            m_previewBodyModel->materials[2].maps[MATERIAL_MAP_DIFFUSE].color = sCol;
        }
        // materials[1] = Areola (GLTF material[0])
        if (m_previewBodyModel->materialCount > 1) {
            m_previewBodyModel->materials[1].maps[MATERIAL_MAP_DIFFUSE].color = Color{
                (unsigned char)(sCol.r * 0.88f), (unsigned char)(sCol.g * 0.73f), (unsigned char)(sCol.b * 0.72f), 255
            };
        }
        // materials[8] = Nipple (GLTF material[7])
        if (m_previewBodyModel->materialCount > 8) {
            m_previewBodyModel->materials[8].maps[MATERIAL_MAP_DIFFUSE].color = Color{
                (unsigned char)(sCol.r * 0.82f), (unsigned char)(sCol.g * 0.63f), (unsigned char)(sCol.b * 0.62f), 255
            };
        }
    }

    // Carica capelli
    const std::string& hStyle = m_appearance.hairStyle;
    if (m_lastLoadedHair != hStyle || !m_previewHairModel || m_lastLoadedGender != m_appearance.gender) {
        m_previewHairModel = nullptr;
        if (!hStyle.empty() && hStyle != "none" && hStyle != "bald") {
            const std::string hairPath = "models/characters/bases/hair_" +
                std::string(isFemale ? "female_" : "male_") + hStyle + ".glb";
            m_previewHairModel = assets->LoadModel(hairPath);
        }
        m_lastLoadedHair = hStyle;
    }

    // Palette capelli
    const int hIdx = std::clamp(m_appearance.hairColorIndex, 0, kHairPaletteCount - 1);
    const Color hCol = kHairPalette[hIdx].color;
    if (m_previewHairModel) {
        for (int m = 0; m < m_previewHairModel->materialCount; ++m) {
            m_previewHairModel->materials[m].maps[MATERIAL_MAP_DIFFUSE].color = hCol;
        }
    }
    // Sopracciglia sul corpo (material 3 = GLTF material[2] = Brows)
    if (m_previewBodyModel && m_previewBodyModel->materialCount > 3) {
        m_previewBodyModel->materials[3].maps[MATERIAL_MAP_DIFFUSE].color = Color{
            (unsigned char)(hCol.r * 0.62f), (unsigned char)(hCol.g * 0.60f), (unsigned char)(hCol.b * 0.58f), 255
        };
    }
}

void CharacterCreationState::Update(float dt) {
    m_previewRotation += 25.0f * dt;
    if (m_previewRotation >= 360.0f) m_previewRotation -= 360.0f;

    if (m_previewBodyModel && m_previewAnims && m_previewIdleIndex >= 0 && m_previewIdleIndex < m_previewAnimCount) {
        const ModelAnimation& anim = m_previewAnims[m_previewIdleIndex];
        if (anim.frameCount > 0 && IsModelAnimationValid(*m_previewBodyModel, anim)) {
            m_previewAnimTime += dt;
            const float dur = 1.933f;
            float phase = std::fmod(m_previewAnimTime, dur) / dur;
            int frame = static_cast<int>(phase * anim.frameCount) % anim.frameCount;
            UpdateModelAnimation(*m_previewBodyModel, anim, frame);
            if (m_previewHairModel && IsModelAnimationValid(*m_previewHairModel, anim)) {
                UpdateModelAnimation(*m_previewHairModel, anim, frame);
            }
        }
    }
}

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
    if (m_page == kPageAppearance) {
        int r = (m_row + dir + kRowAppCount) % kRowAppCount;
        return r;
    }

    int row = m_row;
    for (int i = 0; i < kRowAbiCount; ++i) {
        row = (row + dir + kRowAbiCount) % kRowAbiCount;
        if (row == kRowRoll && !RollRowActive()) continue;
        return row;
    }
    return m_row;
}

void CharacterCreationState::SetMethod(Method method) {
    m_method = method;
    m_assigned.fill(-1);
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
    TraceLog(LOG_INFO, "[CHARACTER] 4d6 drop lowest (auto) -> %d %d %d %d %d %d",
        m_pool[0], m_pool[1], m_pool[2], m_pool[3], m_pool[4], m_pool[5]);
    if (m_row == kRowRoll) m_row = kRowFirstAbility;
}

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
    options.opaqueBackdrop = true;
    options.summary = [](const std::vector<int>& dice) {
        return TextFormat(lang::Get("character.rolled_score").c_str(), core::AbilityScoreFromDice(dice));
    };
    options.onFinish = [game, snap, slot](const std::vector<int>& dice, bool completed) mutable -> std::unique_ptr<GameState> {
        if (completed && dice.size() == 4) {
            snap.pool[static_cast<size_t>(slot)] = core::AbilityScoreFromDice(dice);
            snap.rolled[static_cast<size_t>(slot)] = true;
            TraceLog(LOG_INFO, "[CHARACTER] 4d6 drop lowest (board) slot %d = %d", slot, snap.pool[static_cast<size_t>(slot)]);
        }
        const int rolledNow = static_cast<int>(std::count(snap.rolled.begin(), snap.rolled.end(), true));
        snap.row = (rolledNow < core::kAbilityCount) ? kRowRoll : kRowFirstAbility;
        return std::make_unique<CharacterCreationState>(game, snap);
    };

    m_game->SetState(std::make_unique<DiceRollState>(m_game, core::DiceExpr{ 4, 6 }, std::move(options)));
}

void CharacterCreationState::CycleGender(int dir) {
    (void)dir;
    if (m_appearance.gender == core::Gender::Male) {
        m_appearance.gender = core::Gender::Female;
        m_appearance.hairStyle = kFemaleHairStyles[0].id;
    } else {
        m_appearance.gender = core::Gender::Male;
        m_appearance.hairStyle = kMaleHairStyles[0].id;
    }
    UpdatePreviewVisuals();
}

void CharacterCreationState::CycleHairStyle(int dir) {
    if (m_appearance.gender == core::Gender::Female) {
        int idx = 0;
        for (int i = 0; i < kFemaleHairStyleCount; ++i) {
            if (m_appearance.hairStyle == kFemaleHairStyles[i].id) { idx = i; break; }
        }
        idx = (idx + dir + kFemaleHairStyleCount) % kFemaleHairStyleCount;
        m_appearance.hairStyle = kFemaleHairStyles[idx].id;
    } else {
        int idx = 0;
        for (int i = 0; i < kMaleHairStyleCount; ++i) {
            if (m_appearance.hairStyle == kMaleHairStyles[i].id) { idx = i; break; }
        }
        idx = (idx + dir + kMaleHairStyleCount) % kMaleHairStyleCount;
        m_appearance.hairStyle = kMaleHairStyles[idx].id;
    }
    UpdatePreviewVisuals();
}

void CharacterCreationState::CycleSkinColor(int dir) {
    m_appearance.skinColorIndex = (m_appearance.skinColorIndex + dir + kSkinPaletteCount) % kSkinPaletteCount;
    UpdatePreviewVisuals();
}

void CharacterCreationState::CycleHairColor(int dir) {
    m_appearance.hairColorIndex = (m_appearance.hairColorIndex + dir + kHairPaletteCount) % kHairPaletteCount;
    UpdatePreviewVisuals();
}

void CharacterCreationState::CycleClass(int dir) {
    const int n = core::kCharacterClassCount;
    m_class = static_cast<core::CharacterClass>((static_cast<int>(m_class) + dir + n) % n);
}

void CharacterCreationState::CycleAssignment(int ability, int dir) {
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
    return core::CreateCharacter(m_class, scores, m_appearance);
}

void CharacterCreationState::HandleInput() {
    if (m_editingName) {
        int key = GetCharPressed();
        while (key > 0) {
            if ((key >= 32) && (key <= 125) && (m_appearance.name.length() < 16)) {
                m_appearance.name.push_back((char)key);
            }
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
            if (!m_appearance.name.empty()) m_appearance.name.pop_back();
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) {
            if (m_appearance.name.empty()) m_appearance.name = "Hero";
            m_editingName = false;
        }
        return;
    }

    if (core::KeyBindings::Pressed(core::KeyBindings::Action::Back)) {
        if (m_page == kPageAbilities) {
            m_page = kPageAppearance;
            m_row = kRowAppNext;
            return;
        }
        TraceLog(LOG_INFO, "STATE CHANGE: CharacterCreationState -> MainMenuState");
        if (m_game) m_game->SetState(std::make_unique<MainMenuState>(m_game));
        return;
    }

    if (core::KeyBindings::Pressed(core::KeyBindings::Action::Down)) m_row = NextRow(+1);
    if (core::KeyBindings::Pressed(core::KeyBindings::Action::Up))   m_row = NextRow(-1);

    if (m_page == kPageAppearance) {
        const int dir = (core::KeyBindings::Pressed(core::KeyBindings::Action::Right) ? 1 : 0) -
                        (core::KeyBindings::Pressed(core::KeyBindings::Action::Left) ? 1 : 0);
        if (dir != 0) {
            if (m_row == kRowAppGender) CycleGender(dir);
            else if (m_row == kRowAppHairStyle) CycleHairStyle(dir);
            else if (m_row == kRowAppSkinColor) CycleSkinColor(dir);
            else if (m_row == kRowAppHairColor) CycleHairColor(dir);
        }

        if (core::KeyBindings::Pressed(core::KeyBindings::Action::Confirm)) {
            if (m_row == kRowAppName) {
                m_editingName = true;
            } else if (m_row == kRowAppNext) {
                m_page = kPageAbilities;
                m_row = (m_method == Method::Roll && RollRowActive()) ? kRowRoll : kRowMethod;
            }
        }
        return;
    }

    // Pagina 1: Statistiche
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

    if (core::KeyBindings::Pressed(core::KeyBindings::Action::Confirm)) {
        if (m_row == kRowRoll && RollRowActive()) {
            StartBoardRoll();
            return;
        }
        if (m_row == kRowPrevPage) {
            m_page = kPageAppearance;
            m_row = kRowAppNext;
            return;
        }
        if (m_row == kRowConfirm && AllAssigned()) {
            const core::Character character = Build();
            if (m_game) m_game->StartNewGameLoading(character);
            return;
        }
    }
}

void CharacterCreationState::RenderPreview3D(const Rectangle& viewport) {
    if (!m_previewBodyModel) return;

    // Cornice gothica del riquadro preview a destra
    DrawRectangleRec(viewport, Fade(kWood, 0.6f));
    DrawRectangleLinesEx(viewport, 2.0f, kGoldDim);
    DrawRectangleLinesEx({ viewport.x + 4.0f, viewport.y + 4.0f, viewport.width - 8.0f, viewport.height - 8.0f }, 1.0f, Fade(kGoldDim, 0.4f));

    // Scissor per non sforare dalla finestra preview
    BeginScissorMode((int)viewport.x, (int)viewport.y, (int)viewport.width, (int)viewport.height);

    BeginMode3D(m_previewCamera);

    // Piedistallo / pedana gotica
    DrawCylinder(Vector3{ 0.0f, -0.05f, 0.0f }, 0.55f, 0.60f, 0.1f, 24, Fade(kIron, 0.9f));
    DrawCylinderWires(Vector3{ 0.0f, -0.05f, 0.0f }, 0.55f, 0.60f, 0.1f, 24, kGoldDim);

    const Vector3 drawPos = { 0.0f, 0.0f, 0.0f };
    DrawModelEx(*m_previewBodyModel, drawPos, Vector3{ 0.0f, 1.0f, 0.0f }, m_previewRotation, Vector3{ 1.0f, 1.0f, 1.0f }, WHITE);
    if (m_previewHairModel) {
        DrawModelEx(*m_previewHairModel, drawPos, Vector3{ 0.0f, 1.0f, 0.0f }, m_previewRotation, Vector3{ 1.0f, 1.0f, 1.0f }, WHITE);
    }

    EndMode3D();

    EndScissorMode();

    // Etichetta o decoro sopra/sotto il preview
    const Font& font = UIFont();
    DrawTextCentered(font, m_appearance.name.c_str(), viewport.x + viewport.width / 2.0f, viewport.y + 14.0f, 20.0f, kGoldBright);
}

void CharacterCreationState::Render() {
    const Font& font = UIFont();
    const float sw = static_cast<float>(GetScreenWidth());
    const float sh = static_cast<float>(GetScreenHeight());

    // Layout gotico responsive:
    // Pannello opzioni a sinistra, anteprima 3D a destra
    const float totalMargin = 28.0f;
    const float panelH = std::clamp(sh * 0.90f, 600.0f, 760.0f);
    const float panelY = (sh - panelH) / 2.0f;

    // Se lo schermo è largo (>= 1100 px), affianchiamo il preview 3D a destra.
    // Altrimenti (schermo stretto) usiamo il layout classico centrato.
    const bool splitView = (sw >= 1050.0f);
    float panelW = splitView ? std::clamp(sw * 0.52f, 520.0f, 680.0f) : std::clamp(sw * 0.58f, 580.0f, 760.0f);
    float panelX = splitView ? totalMargin + 16.0f : (sw - panelW) / 2.0f;

    const Rectangle b = { panelX, panelY, panelW, panelH };
    const float cx = b.x + b.width / 2.0f;
    const float left = b.x + 32.0f;
    const float rowW = b.width - 64.0f;
    const float kRowH = 34.0f;

    // Sfondo pieno gotico
    DrawRectangle(0, 0, (int)sw, (int)sh, kIron);

    // Render Preview 3D a destra se c'è spazio
    if (splitView) {
        const float previewX = b.x + b.width + 24.0f;
        const float previewW = (sw - previewX) - totalMargin;
        const Rectangle previewRect = { previewX, panelY, previewW, panelH };
        RenderPreview3D(previewRect);
    }

    // Cornice gotica esterna del pannello opzioni
    DrawRectangleRec(b, kWood);
    DrawRectangleLinesEx(b, 3.0f, kGoldDim);
    DrawRectangleLinesEx({ b.x + 8.0f, b.y + 8.0f, b.width - 16.0f, b.height - 16.0f }, 1.0f, Fade(kGoldDim, 0.7f));

    // Intestazione
    DrawTextCentered(font, lang::Get("character.title").c_str(), cx, b.y + 20.0f, 30.0f, kGoldBright);

    // Titolo pagina & indicatore gotico (1/2 o 2/2)
    const char* pageSubtitle = (m_page == kPageAppearance)
        ? lang::Get("character.page_appearance").c_str()
        : lang::Get("character.page_abilities").c_str();
    DrawTextCentered(font, TextFormat("%s  [%d / 2]", pageSubtitle, m_page + 1), cx, b.y + 56.0f, 20.0f, Fade(kBone, 0.85f));

    auto rowColor = [&](int row) { return (m_row == row) ? kGoldBright : kBone; };

    if (m_page == kPageAppearance) {
        float y = b.y + 104.0f;

        // --- Nome ---
        if (m_row == kRowAppName) DrawRowHighlight({ left - 10.0f, y - 4.0f, rowW + 20.0f, kRowH - 2.0f });
        DrawLabel(font, lang::Get("character.name").c_str(), left, y, 22.0f, rowColor(kRowAppName));
        std::string nameDisplay = m_appearance.name;
        if (m_editingName) nameDisplay += "_";
        DrawLabel(font, TextFormat("[ %s ]", nameDisplay.c_str()), left + 200.0f, y, 22.0f,
                  m_editingName ? kGoldBright : rowColor(kRowAppName));
        if (m_row == kRowAppName && !m_editingName) {
            DrawLabel(font, lang::Get("character.edit_name_hint").c_str(), left + 390.0f, y + 3.0f, 16.0f, Fade(kBone, 0.65f));
        }
        y += kRowH + 12.0f;

        // --- Sesso ---
        if (m_row == kRowAppGender) DrawRowHighlight({ left - 10.0f, y - 4.0f, rowW + 20.0f, kRowH - 2.0f });
        DrawLabel(font, lang::Get("character.gender").c_str(), left, y, 22.0f, rowColor(kRowAppGender));
        const std::string genderStr = lang::Get(m_appearance.gender == core::Gender::Female ? "character.gender_female" : "character.gender_male");
        DrawLabel(font, TextFormat("<  %s  >", genderStr.c_str()), left + 200.0f, y, 22.0f, rowColor(kRowAppGender));
        y += kRowH + 12.0f;

        // --- Capelli ---
        if (m_row == kRowAppHairStyle) DrawRowHighlight({ left - 10.0f, y - 4.0f, rowW + 20.0f, kRowH - 2.0f });
        DrawLabel(font, lang::Get("character.hair_style").c_str(), left, y, 22.0f, rowColor(kRowAppHairStyle));
        std::string hairLabel = m_appearance.hairStyle;
        if (m_appearance.gender == core::Gender::Female) {
            for (int i = 0; i < kFemaleHairStyleCount; ++i) {
                if (m_appearance.hairStyle == kFemaleHairStyles[i].id) { hairLabel = kFemaleHairStyles[i].label; break; }
            }
        } else {
            for (int i = 0; i < kMaleHairStyleCount; ++i) {
                if (m_appearance.hairStyle == kMaleHairStyles[i].id) { hairLabel = kMaleHairStyles[i].label; break; }
            }
        }
        DrawLabel(font, TextFormat("<  %s  >", hairLabel.c_str()), left + 200.0f, y, 22.0f, rowColor(kRowAppHairStyle));
        y += kRowH + 12.0f;

        // --- Colore Pelle ---
        if (m_row == kRowAppSkinColor) DrawRowHighlight({ left - 10.0f, y - 4.0f, rowW + 20.0f, kRowH - 2.0f });
        DrawLabel(font, lang::Get("character.skin_color").c_str(), left, y, 22.0f, rowColor(kRowAppSkinColor));
        const int skinIdx = std::clamp(m_appearance.skinColorIndex, 0, kSkinPaletteCount - 1);
        DrawLabel(font, TextFormat("<  %s  >", kSkinPalette[skinIdx].name), left + 200.0f, y, 22.0f, rowColor(kRowAppSkinColor));
        DrawRectangleRec({ left + 390.0f, y + 2.0f, 26.0f, 20.0f }, kSkinPalette[skinIdx].color);
        DrawRectangleLinesEx({ left + 390.0f, y + 2.0f, 26.0f, 20.0f }, 1.0f, kGoldBright);
        y += kRowH + 12.0f;

        // --- Colore Capelli ---
        if (m_row == kRowAppHairColor) DrawRowHighlight({ left - 10.0f, y - 4.0f, rowW + 20.0f, kRowH - 2.0f });
        DrawLabel(font, lang::Get("character.hair_color").c_str(), left, y, 22.0f, rowColor(kRowAppHairColor));
        const int hairIdx = std::clamp(m_appearance.hairColorIndex, 0, kHairPaletteCount - 1);
        DrawLabel(font, TextFormat("<  %s  >", kHairPalette[hairIdx].name), left + 200.0f, y, 22.0f, rowColor(kRowAppHairColor));
        DrawRectangleRec({ left + 390.0f, y + 2.0f, 26.0f, 20.0f }, kHairPalette[hairIdx].color);
        DrawRectangleLinesEx({ left + 390.0f, y + 2.0f, 26.0f, 20.0f }, 1.0f, kGoldBright);
        y += kRowH + 40.0f;

        // Bottone Avanti
        if (m_row == kRowAppNext) DrawRowHighlight({ left - 10.0f, y - 4.0f, rowW + 20.0f, kRowH + 6.0f });
        DrawTextCentered(font, lang::Get("character.next_page").c_str(), cx, y, 24.0f, rowColor(kRowAppNext));

        DrawTextCentered(font, lang::Get("character.help").c_str(), cx, b.y + b.height + 14.0f, 18.0f, Fade(kBone, 0.7f));
        return;
    }

    // --- PAGINA 1: STATISTICHE ---
    static const core::Character kSheet{};
    const std::string raceName = lang::RaceName(core::ToString(kSheet.race));
    DrawTextCentered(font, TextFormat("%s (%s)  -  %s", m_appearance.name.c_str(),
                     lang::Get(m_appearance.gender == core::Gender::Female ? "character.gender_female" : "character.gender_male").c_str(),
                     TextFormat(lang::Get("character.sheet").c_str(), raceName.c_str(), kSheet.level, kSheet.xp)),
                     cx, b.y + 84.0f, 18.0f, Fade(kBone, 0.8f));

    // Metodo
    float y = b.y + 116.0f;
    if (m_row == kRowMethod) DrawRowHighlight({ left - 10.0f, y - 4.0f, rowW + 20.0f, kRowH - 2.0f });
    DrawLabel(font, lang::Get("character.scores").c_str(), left, y, 22.0f, rowColor(kRowMethod));
    const std::string methodName = MethodName(m_method);
    DrawLabel(font, TextFormat("<  %s  >", methodName.c_str()), left + 180.0f, y, 22.0f, rowColor(kRowMethod));

    // Classe
    y += kRowH;
    if (m_row == kRowClass) DrawRowHighlight({ left - 10.0f, y - 4.0f, rowW + 20.0f, kRowH - 2.0f });
    DrawLabel(font, lang::Get("character.class").c_str(), left, y, 22.0f, rowColor(kRowClass));
    const std::string className = lang::CharacterClassName(core::ToString(m_class));
    DrawLabel(font, TextFormat("<  %s  >", className.c_str()), left + 180.0f, y, 22.0f, rowColor(kRowClass));

    // Pool dei punteggi
    y += kRowH + 6.0f;
    DrawLabel(font, lang::Get(m_method == Method::Roll ? "character.rolls_label" : "character.array_label").c_str(),
              left, y, 18.0f, Fade(kBone, 0.75f));
    y += 24.0f;
    for (int i = 0; i < core::kAbilityCount; ++i) {
        const Rectangle slot = { left + 4.0f + i * 82.0f, y, 66.0f, 30.0f };
        const bool known = m_rolled[static_cast<size_t>(i)];
        const bool used = known && IsPoolSlotUsed(i);
        DrawRectangleRec(slot, Fade(BLACK, used ? 0.25f : 0.45f));
        DrawRectangleLinesEx(slot, 1.0f, Fade(kGoldDim, (used || !known) ? 0.35f : 0.9f));
        const char* t = known ? TextFormat("%d", m_pool[static_cast<size_t>(i)]) : "?";
        const Vector2 m = MeasureTextEx(font, t, 20.0f, 2.0f);
        DrawLabel(font, t, slot.x + (slot.width - m.x) / 2.0f, slot.y + 4.0f, 20.0f,
                  !known ? Fade(kBone, 0.3f) : (used ? Fade(kBone, 0.3f) : kGoldBright));
    }

    // Lancio dadi
    y += 30.0f + 6.0f;
    if (m_method == Method::Roll) {
        if (RollRowActive()) {
            if (m_row == kRowRoll) DrawRowHighlight({ left - 10.0f, y - 4.0f, rowW + 20.0f, kRowH - 2.0f });
            DrawLabel(font, TextFormat(lang::Get("character.roll_action").c_str(), RolledCount(), core::kAbilityCount), left, y, 20.0f, rowColor(kRowRoll));
            DrawLabel(font, lang::Get("character.roll_help").c_str(), left + 240.0f, y + 2.0f, 16.0f, Fade(kBone, 0.65f));
        } else {
            DrawLabel(font, lang::Get("character.all_rolled").c_str(), left, y, 18.0f, Fade(kBone, 0.6f));
        }
    }
    y += kRowH;

    // Caratteristiche
    y += 4.0f;
    for (int a = 0; a < core::kAbilityCount; ++a) {
        const int row = kRowFirstAbility + a;
        if (m_row == row) DrawRowHighlight({ left - 10.0f, y - 4.0f, rowW + 20.0f, kRowH - 2.0f });
        const int slot = m_assigned[static_cast<size_t>(a)];
        DrawLabel(font, core::ToString(static_cast<core::Ability>(a)), left, y, 22.0f, rowColor(row));
        if (slot >= 0) {
            const int score = m_pool[static_cast<size_t>(slot)];
            DrawLabel(font, TextFormat("<  %d  >", score), left + 180.0f, y, 22.0f, rowColor(row));
            DrawLabel(font, TextFormat("%+d", core::AbilityModifier(score)), left + 340.0f, y, 22.0f, rowColor(row));
        } else {
            DrawLabel(font, "<  --  >", left + 180.0f, y, 22.0f, Fade(rowColor(row), 0.6f));
        }
        y += kRowH;
    }

    // Arma iniziale
    y += 4.0f;
    const core::ItemId weaponId = core::StartingWeaponId(m_class);
    const core::WeaponDef weapon = core::ResolveWeapon(weaponId);
    const core::ItemDef* weaponDef = core::GetItem(weaponId);
    const std::string weaponName = weaponDef ? lang::ItemName(weaponDef->key) : weapon.name;
    DrawLabel(font, TextFormat(lang::Get("character.initial_weapon").c_str(), weaponName.c_str(), core::ToString(weapon.damage).c_str()),
              left, y, 18.0f, Fade(kBone, 0.85f));

    // Navigazione / Conferma
    y += 30.0f;
    const bool ready = AllAssigned();
    if (m_row == kRowConfirm) DrawRowHighlight({ left - 10.0f, y - 4.0f, rowW + 20.0f, kRowH - 2.0f });
    DrawTextCentered(font, lang::Get(ready ? "character.confirm" : "character.confirm_assign").c_str(), cx, y, 22.0f,
                     ready ? rowColor(kRowConfirm) : Fade(kBone, 0.4f));

    y += kRowH;
    if (m_row == kRowPrevPage) DrawRowHighlight({ left - 10.0f, y - 4.0f, rowW + 20.0f, kRowH - 2.0f });
    DrawTextCentered(font, lang::Get("character.prev_page").c_str(), cx, y, 20.0f, rowColor(kRowPrevPage));

    DrawTextCentered(font, lang::Get("character.help").c_str(), cx, b.y + b.height + 14.0f, 18.0f, Fade(kBone, 0.7f));
}
