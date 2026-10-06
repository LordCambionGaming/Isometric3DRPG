#include "states/DiceRollState.h"
#include "states/PlayingState.h"
#include "core/Game.h"
#include "entities/Mob.h"
#include "ui/GothicTheme.h"
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

using namespace ui::theme;

namespace {

// Regole del colpo (per ora fisse: quando ci sara' un'arma equipaggiata andranno lette da li)
const core::DiceExpr kWeaponDice{ 1, 6 };
constexpr int   kAttackBonus    = 0;
constexpr int   kDamageModifier = 2;
constexpr int   kMaxPoolDice    = 8;      // oltre, i dadi si tirano col vecchio percorso semplice
constexpr float kPendingDelay   = 1.1f;   // pausa per leggere il risultato prima del dado successivo

std::string JoinValues(const std::vector<int>& v) {
    std::string s;
    for (size_t i = 0; i < v.size(); ++i) s += (i ? " + " : "") + std::to_string(v[i]);
    return s;
}

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

// ---------------------------------------------------------------------------
// Costruzione
// ---------------------------------------------------------------------------

DiceRollState::DiceRollState(Game* game, Mob* target)
    : GameState(game)
    , m_target(target)
    , m_diceExpr({ 1, 20 })
    , m_isCombatRoll(true)
{
    m_type = GameStateType::DiceRoll;
    SetupGothicBoard();
}

DiceRollState::DiceRollState(Game* game, core::DiceExpr expr)
    : GameState(game)
    , m_target(nullptr)
    , m_diceExpr(expr)
    , m_isCombatRoll(false)
{
    m_type = GameStateType::DiceRoll;
    SetupGothicBoard();
}

DiceRollState::DiceRollState(Game* game, core::DiceExpr expr, DiceRollOptions options)
    : GameState(game)
    , m_target(nullptr)
    , m_diceExpr(expr)
    , m_isCombatRoll(false)
    , m_opts(std::move(options))
{
    m_type = GameStateType::DiceRoll;
    SetupGothicBoard();
}

DiceRollState::~DiceRollState() = default;

// d4/d6/d8/d10/d12/d20 (fino a kMaxPoolDice dadi) -> board gotica. Il resto -> dadi semplici.
void DiceRollState::SetupGothicBoard() {
    m_gothic = ui::IsSupportedDie(m_diceExpr.sides) &&
               m_diceExpr.count >= 1 && m_diceExpr.count <= kMaxPoolDice;
    if (!m_gothic) return;

    m_phase = IsCombat() ? Phase::Attack : Phase::Free;
    m_pool = m_diceExpr;
    m_dice.SetSides(m_pool.sides);
    m_dice.SetRollProvider([this]() -> core::CombatRoll { return ThrowGothicDie(); });
}

// Il vero tiro viene deciso qui, nell'istante in cui lanci il dado; la faccia finale lo mostra.
core::CombatRoll DiceRollState::ThrowGothicDie() {
    core::CombatRoll roll;

    if (m_phase == Phase::Attack) {
        core::CombatSystem* cs = m_game ? m_game->GetCombatSystem() : nullptr;
        if (cs) {
            m_attackResult = cs->RollAttack(kAttackBonus, m_target->GetArmorClass());
            m_attackRolled = true;
            roll.rollValue = m_attackResult.d20Roll;
            roll.result = m_attackResult.resultType;
            roll.description = m_attackResult.log;
            return roll;
        }
    }

    // un tiro libero gia' completato, se lo rilanci, ricomincia da capo
    if (m_phase == Phase::Free && m_poolDone) {
        m_poolValues.clear();
        m_poolDone = false;
    }

    const int v = core::RollDie(m_pool.sides);
    roll.rollValue = v;
    roll.result = core::CombatResult::Normal;
    if (m_pool.sides == 20) {
        if (v == 1)  roll.result = core::CombatResult::CriticalFailure;
        if (v == 20) roll.result = core::CombatResult::CriticalSuccess;
    }
    return roll;
}

core::DiceExpr DiceRollState::DamagePool() const {
    const bool crit = m_attackRolled && m_attackResult.resultType == core::CombatResult::CriticalSuccess;
    return core::DiceExpr{ kWeaponDice.count * (crit ? 2 : 1), kWeaponDice.sides };
}

// Mette sul feltro il dado giusto (cambia solido e atlas se serve), pronto per essere afferrato.
void DiceRollState::BeginPool(const core::DiceExpr& pool) {
    m_pool = pool;
    m_poolValues.clear();
    m_poolDone = false;
    m_dice.SetSides(pool.sides);
    m_dice.Reset();
    m_showResult = false;
}

void DiceRollState::FinishDamage() {
    core::CombatSystem* cs = m_game ? m_game->GetCombatSystem() : nullptr;
    if (cs) {
        m_damageResult = cs->ResolveDamage(kWeaponDice, m_poolValues, kDamageModifier);
        m_damageRolled = true;
    }
    m_combatDone = true;
}

// Chiamata quando il dado si e' fermato E allineato sulla faccia: decide cosa succede dopo.
void DiceRollState::OnThrowResolved(const core::CombatRoll& r) {
    m_result = r;
    m_showResult = true;

    switch (m_phase) {
        case Phase::Attack:
            if (m_attackRolled && m_attackResult.isHit) {
                m_pending = Pending::ToDamage;      // colpito: ora si tira il dado del danno
                m_pendingTimer = 0.0f;
            } else {
                m_combatDone = true;                // mancato / fallimento critico: finito
            }
            break;

        case Phase::Damage:
            m_poolValues.push_back(r.rollValue);
            if ((int)m_poolValues.size() < m_pool.count) {
                m_pending = Pending::ReArm;         // colpo critico: altro dado del danno
                m_pendingTimer = 0.0f;
            } else {
                FinishDamage();
            }
            break;

        case Phase::Free:
            m_poolValues.push_back(r.rollValue);
            if ((int)m_poolValues.size() < m_pool.count) {
                m_pending = Pending::ReArm;         // NdX: prossimo dado
                m_pendingTimer = 0.0f;
            } else {
                m_poolDone = true;
                m_diceResult.dice = m_poolValues;
                m_diceResult.total = 0;
                for (int v : m_poolValues) m_diceResult.total += v;
            }
            break;
    }
}

Rectangle DiceRollState::Board() const {
    const float w = 720.0f, h = 450.0f;
    return { (GetScreenWidth() - w) / 2.0f, (GetScreenHeight() - h) / 2.0f + 10.0f, w, h };
}

Rectangle DiceRollState::Felt() const {
    const Rectangle b = Board();
    return { b.x + 30.0f, b.y + 84.0f, b.width - 60.0f, b.height - 84.0f - 34.0f };
}

// ---------------------------------------------------------------------------
// Ciclo di vita
// ---------------------------------------------------------------------------

void DiceRollState::Enter() {
    if (!m_game) return;

    if (m_gothic) {
        DiceFont();                         // carica il font dei dadi ora, non a meta' frame
        m_dice.SetBounds(Felt());
        m_result = core::CombatRoll{};
        m_time = 0.0f;
        m_attackRolled = m_damageRolled = m_damageApplied = m_combatDone = false;
        m_pending = Pending::None;
        m_phase = IsCombat() ? Phase::Attack : Phase::Free;
        BeginPool(m_diceExpr);
        return;
    }

    // Camera 3D dedicata alla vista dall'alto sul vassoio dadi
    m_diceCamera.position = Vector3{ 0.0f, 10.0f, 6.0f };
    m_diceCamera.target = Vector3{ 0.0f, 0.0f, 0.0f };
    m_diceCamera.up = Vector3{ 0.0f, 1.0f, 0.0f };
    m_diceCamera.fovy = 45.0f;
    m_diceCamera.projection = CAMERA_PERSPECTIVE;

    m_diceResult = core::RollDice(m_diceExpr);
    InitDicePhysics();
}

void DiceRollState::Exit() {
    // Il danno si applica una sola volta, e solo se l'attacco e' stato lanciato davvero e ha colpito.
    if (!(IsCombat() && m_attackRolled && m_attackResult.isHit) || m_damageApplied) return;

    if (!m_damageRolled) {
        // Hai chiuso la board dopo aver colpito ma prima di aver lanciato tutti i dadi del danno:
        // un colpo a segno non si perde, i dadi che mancano li tira il gioco.
        core::CombatSystem* cs = m_game ? m_game->GetCombatSystem() : nullptr;
        if (cs) {
            const core::DiceExpr pool = DamagePool();
            std::vector<int> dice = (m_phase == Phase::Damage) ? m_poolValues : std::vector<int>{};
            while ((int)dice.size() < pool.count) dice.push_back(core::RollDie(pool.sides));
            m_damageResult = cs->ResolveDamage(kWeaponDice, dice, kDamageModifier);
            m_damageRolled = true;
        }
    }
    if (m_damageRolled) {
        m_damageApplied = true;
        m_target->TakeDamage(m_damageResult.totalDamage);
    }
}

void DiceRollState::Update(float deltaTime) {
    if (m_gothic) UpdateGothic(deltaTime);
    else          UpdateSimple(deltaTime);
}

void DiceRollState::Render() {
    if (m_gothic) RenderGothic();
    else          RenderSimple();
}

void DiceRollState::HandleInput() {
    if (m_gothic) HandleInputGothic();
    else          HandleInputSimple();
}

// ---------------------------------------------------------------------------
// Board gotica
// ---------------------------------------------------------------------------

void DiceRollState::UpdateGothic(float deltaTime) {
    m_time += deltaTime;
    m_dice.Update(deltaTime);

    // pausa tra un lancio e il successivo, poi si prepara il dado giusto
    if (m_pending != Pending::None) {
        m_pendingTimer += deltaTime;
        if (m_pendingTimer >= kPendingDelay && !m_dice.IsBusy()) {
            const Pending p = m_pending;
            m_pending = Pending::None;
            if (p == Pending::ToDamage) {
                m_phase = Phase::Damage;
                BeginPool(DamagePool());            // d6 del danno (due se critico)
            } else {
                m_dice.Reset();                     // stesso tipo di dado, lancio successivo
                m_showResult = false;
            }
        }
    }

    if (m_dice.IsBusy()) m_showResult = false;

    core::CombatRoll r;
    if (m_dice.ConsumeResult(r)) OnThrowResolved(r);
}

void DiceRollState::RenderGothic() {
    const Font& font = UIFont();
    const int sw = GetScreenWidth(), sh = GetScreenHeight();
    const Rectangle b = Board();
    const Rectangle felt = Felt();
    const float cx = b.x + b.width / 2.0f;

    // il mondo congelato dietro si oscura (oppure sfondo pieno, se dietro non c'e' il mondo)
    if (m_opts.opaqueBackdrop) DrawRectangle(0, 0, sw, sh, kIron);
    else                       DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.75f));

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
    const int have = (int)m_poolValues.size();
    const char* banner = "";
    Color bannerColor = kBone;
    std::string subline;

    if (m_showResult) {
        switch (m_phase) {
            case Phase::Attack: {
                const char* label = "Tiro normale";
                if (m_result.result == core::CombatResult::CriticalSuccess) {
                    label = "Successo critico!"; bannerColor = kGoldBright;
                } else if (m_result.result == core::CombatResult::CriticalFailure) {
                    label = "Fallimento critico!"; bannerColor = kBlood;
                }
                if (m_attackRolled) {
                    // esito dell'attacco calcolato da CombatSystem (Colpito! / Mancato! / critici)
                    label = m_attackResult.log.c_str();
                    if (m_result.result == core::CombatResult::Normal && !m_attackResult.isHit) {
                        bannerColor = Fade(kBone, 0.6f);
                    }
                }
                banner = TextFormat("%d  -  %s", m_result.rollValue, label);
                if (m_pending == Pending::ToDamage) subline = "Ora tira il dado del danno";
                break;
            }
            case Phase::Damage:
                if (m_damageRolled) {
                    banner = TextFormat("%s", m_damageResult.log.c_str());
                    bannerColor = kGoldBright;
                    subline = "Dadi: " + JoinValues(m_poolValues) + "   Modificatore: +" + std::to_string(kDamageModifier);
                } else {
                    banner = TextFormat("Dado %d/%d:  %d", have, m_pool.count, m_result.rollValue);
                    subline = "Dadi: " + JoinValues(m_poolValues);
                }
                break;
            case Phase::Free:
                if (m_pool.count == 1) {
                    if (m_pool.sides == 20) {
                        const char* label = "Tiro normale";
                        if (m_result.result == core::CombatResult::CriticalSuccess) {
                            label = "Successo critico!"; bannerColor = kGoldBright;
                        } else if (m_result.result == core::CombatResult::CriticalFailure) {
                            label = "Fallimento critico!"; bannerColor = kBlood;
                        }
                        banner = TextFormat("%d  -  %s", m_result.rollValue, label);
                    } else {
                        banner = TextFormat("%d  -  d%d", m_result.rollValue, m_pool.sides);
                    }
                } else if (m_poolDone) {
                    if (m_opts.summary) banner = TextFormat("%s", m_opts.summary(m_poolValues).c_str());
                    else                banner = TextFormat("Risultato %s: %d", core::ToString(m_pool).c_str(), m_diceResult.total);
                    bannerColor = kGoldBright;
                    subline = "Dadi: " + JoinValues(m_poolValues);
                } else {
                    banner = TextFormat("Dado %d/%d:  %d", have, m_pool.count, m_result.rollValue);
                    subline = "Dadi: " + JoinValues(m_poolValues);
                }
                break;
        }
    } else {
        const int next = (m_phase == Phase::Free && m_poolDone) ? 1 : have + 1;
        switch (m_dice.GetState()) {
            case ui::DiceRoller::State::Idle:
                if (m_phase == Phase::Damage) {
                    banner = (m_pool.count == 1) ? "Colpito! Lancia il dado del danno"
                                                 : TextFormat("Lancia il dado del danno (%d/%d)", next, m_pool.count);
                } else if (m_phase == Phase::Free && m_pool.count > 1) {
                    banner = TextFormat("Lancia il dado (%d/%d)", next, m_pool.count);
                } else {
                    banner = "Afferra il dado e scaglialo sul feltro";
                }
                break;
            case ui::DiceRoller::State::Dragging: banner = "Lascia andare con uno scatto!"; break;
            default:                              banner = "Il destino rotola...";          break;
        }
        bannerColor = Fade(kBone, 0.75f);
        if (m_phase != Phase::Attack && have > 0 && !m_poolDone) subline = "Dadi: " + JoinValues(m_poolValues);
    }
    DrawTextCentered(font, banner, cx, b.y + 30.0f, 32.0f, bannerColor);
    if (!subline.empty()) DrawTextCentered(font, subline.c_str(), cx, b.y + 62.0f, 20.0f, kGold);

    // --- dado ------------------------------------------------------------
    m_dice.Render();

    // --- pie' di pagina --------------------------------------------------
    DrawTextCentered(font, "INVIO / ESC : chiudi", cx, b.y + b.height + 14.0f, 22.0f, Fade(kBone, 0.7f));
}

void DiceRollState::HandleInputGothic() {
    // Durante la pausa tra due lanci e dopo la fine di un attacco il dado non si puo' afferrare
    // (un attacco si lancia una volta sola: niente rilanci finche' non colpisci).
    const bool lockedByCaller = m_opts.onFinish && m_opts.lockWhenDone && m_poolDone;
    const bool locked = (m_pending != Pending::None) || (IsCombat() && m_combatDone) || lockedByCaller;
    if (!locked) {
        m_dice.HandleInput();
    }

    if (!m_dice.IsBusy() && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE))) {
        // Lo stato di destinazione si costruisce PRIMA di SetState: SetState distrugge questo oggetto.
        std::unique_ptr<GameState> next;
        if (m_opts.onFinish) next = m_opts.onFinish(m_poolDone ? m_poolValues : std::vector<int>{}, m_poolDone);
        else                 next = std::make_unique<PlayingState>(m_game);
        m_game->SetState(std::move(next));
        return;   // questo stato e' stato distrutto: non toccare i membri
    }
}

// ---------------------------------------------------------------------------
// Dadi semplici (1d4, 2d6, 3d20, ...) - invariato rispetto alla versione attuale
// ---------------------------------------------------------------------------

void DiceRollState::InitDicePhysics() {
    m_visualDice.clear();
    const size_t count = m_diceResult.dice.size();

    for (size_t i = 0; i < count; ++i) {
        VisualDice3D die;
        die.sides = m_diceExpr.sides;
        die.finalValue = m_diceResult.dice[i];

        // Posizione iniziale rialzata e sfalsata per ogni dado
        float offsetX = (static_cast<float>(i) - static_cast<float>(count - 1) / 2.0f) * 2.2f;
        die.position = Vector3{ offsetX, 6.0f + static_cast<float>(core::RollDie(10)) * 0.2f, (core::RollDie(5) - 3) * 0.5f };

        // Velocita' iniziale di lancio e rotazione casuale
        die.velocity = Vector3{ (core::RollDie(10) - 5) * 0.4f, -6.0f, (core::RollDie(10) - 5) * 0.4f };
        die.angularVelocity = Vector3{ static_cast<float>(core::RollDie(360)), static_cast<float>(core::RollDie(360)), static_cast<float>(core::RollDie(360)) };

        // Colore differenziato per tipologia di dado
        switch (die.sides) {
            case 4:  die.color = PURPLE; break;
            case 6:  die.color = RED; break;
            case 8:  die.color = ORANGE; break;
            case 10: die.color = GOLD; break;
            case 12: die.color = BLUE; break;
            case 20: die.color = MAROON; break;
            default: die.color = DARKGRAY; break;
        }

        m_visualDice.push_back(die);
    }
}

void DiceRollState::UpdateSimple(float deltaTime) {
    m_stateTimer += deltaTime;
    bool allSettledThisFrame = true;

    for (auto& die : m_visualDice) {
        if (die.settled) continue;

        // Gravita' e movimento
        die.velocity.y -= 18.0f * deltaTime;
        die.position = Vector3Add(die.position, Vector3Scale(die.velocity, deltaTime));
        die.rotation = Vector3Add(die.rotation, Vector3Scale(die.angularVelocity, deltaTime));

        // Rimbalzo sul piano del tavolo (Y = 0)
        if (die.position.y <= 0.6f) {
            die.position.y = 0.6f;
            die.velocity.y = -die.velocity.y * 0.45f; // Smorzamento rimbalzo
            die.velocity.x *= 0.6f;
            die.velocity.z *= 0.6f;
            die.angularVelocity = Vector3Scale(die.angularVelocity, 0.5f);

            // Arresto definitivo del dado quando la velocita' e' minima
            if (Vector3Length(die.velocity) < 0.6f) {
                die.settled = true;
                die.velocity = Vector3Zero();
                die.angularVelocity = Vector3Zero();
                // Orientamento finale piatto
                die.rotation = Vector3{ 0.0f, static_cast<float>(core::RollDie(4) * 90), 0.0f };
            }
        }

        if (!die.settled) allSettledThisFrame = false;
    }

    m_allSettled = allSettledThisFrame || (m_stateTimer >= 2.5f);

    // Torna automaticamente al gioco dopo che tutti i dadi si sono fermati
    if (m_allSettled && m_stateTimer >= 4.0f) {
        m_game->SetState(std::make_unique<PlayingState>(m_game));
    }
}

void DiceRollState::RenderSimple() {
    // Sfondo e rendering della scena 3D
    BeginMode3D(m_diceCamera);

    // Tavolo / Vassoio dei dadi
    DrawPlane(Vector3{ 0.0f, 0.0f, 0.0f }, Vector2{ 16.0f, 12.0f }, DARKGREEN);
    DrawCubeWires(Vector3{ 0.0f, 0.0f, 0.0f }, 16.2f, 0.2f, 12.2f, BROWN);

    // Renderizza ogni dado 3D in base al numero di facce
    for (const auto& die : m_visualDice) {
        RenderDie3D(die);
    }

    EndMode3D();

    // HUD / Overlay Risultati 2D
    const int cx = GetScreenWidth() / 2;

    if (m_allSettled) {
        const char* exprStr = TextFormat("Risultato %s: %d", core::ToString(m_diceExpr).c_str(), m_diceResult.total);
        DrawText(exprStr, cx - MeasureText(exprStr, 36) / 2, 80, 36, GOLD);

        DrawText("Premi [SPAZIO] o [INVIO] per continuare", cx - MeasureText("Premi [SPAZIO] o [INVIO] per continuare", 20) / 2, GetScreenHeight() - 60, 20, LIGHTGRAY);
    } else {
        DrawText("Lancio in corso...", cx - MeasureText("Lancio in corso...", 28) / 2, 80, 28, WHITE);
    }
}

void DiceRollState::RenderDie3D(const VisualDice3D& die) {
    rlPushMatrix();
    rlTranslatef(die.position.x, die.position.y, die.position.z);
    rlRotatef(die.rotation.x, 1.0f, 0.0f, 0.0f);
    rlRotatef(die.rotation.y, 0.0f, 1.0f, 0.0f);
    rlRotatef(die.rotation.z, 0.0f, 0.0f, 1.0f);

    // Geometria 3D specifica per tipo di dado (d4, d6, d8, d10, d12, d20)
    switch (die.sides) {
        case 4:
            // Tetraedro (D4)
            DrawCylinderWiresEx(Vector3{0, -0.4f, 0}, Vector3{0, 0.5f, 0}, 0.8f, 0.0f, 3, die.color);
            break;
        case 6:
            // Cubo (D6)
            DrawCube(Vector3Zero(), 1.2f, 1.2f, 1.2f, die.color);
            DrawCubeWires(Vector3Zero(), 1.2f, 1.2f, 1.2f, BLACK);
            break;
        case 8:
            // Ottaedro (D8)
            DrawCylinderWiresEx(Vector3{0, -0.7f, 0}, Vector3{0, 0.7f, 0}, 0.8f, 0.0f, 4, die.color);
            break;
        case 10:
        case 12:
            // Dodecaedro / Trapezoedro (D10/D12)
            DrawSphereEx(Vector3Zero(), 0.8f, 3, 5, die.color);
            DrawSphereWires(Vector3Zero(), 0.81f, 3, 5, BLACK);
            break;
        case 20:
        default:
            // Icosaedro (D20) - qui solo per pool di piu' d20
            DrawSphereEx(Vector3Zero(), 0.9f, 4, 6, die.color);
            DrawSphereWires(Vector3Zero(), 0.91f, 4, 6, GOLD);
            break;
    }

    rlPopMatrix();

    // Numero proiettato sopra il dado quando fermo
    if (die.settled) {
        Vector2 screenPos = GetWorldToScreen(Vector3{ die.position.x, die.position.y + 0.9f, die.position.z }, m_diceCamera);
        const char* valStr = TextFormat("%d", die.finalValue);
        DrawText(valStr, (int)screenPos.x - MeasureText(valStr, 22) / 2, (int)screenPos.y - 11, 22, WHITE);
    }
}

void DiceRollState::HandleInputSimple() {
    // Tasto rapido per saltare la fisica del tiro e mostrare subito l'esito
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
        if (!m_allSettled) {
            for (auto& die : m_visualDice) {
                die.settled = true;
                die.position.y = 0.6f;
                die.velocity = Vector3Zero();
                die.angularVelocity = Vector3Zero();
            }
            m_allSettled = true;
        } else {
            m_game->SetState(std::make_unique<PlayingState>(m_game));
        }
    }
}