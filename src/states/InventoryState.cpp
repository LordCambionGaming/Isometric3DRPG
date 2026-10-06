#include "states/InventoryState.h"
#include "states/PlayingState.h"
#include "core/Game.h"
#include "entities/Player.h"
#include "ui/GothicTheme.h"
#include <raylib.h>
#include <memory>

using namespace ui::theme;

InventoryState::InventoryState(Game* game) : GameState(game) {
    m_type = GameStateType::Inventory;
    if (Player* p = m_game->GetPlayer()) {
        const auto& c = p->GetCharacter();
        if (c.HasEquippedWeapon()) m_selected = c.equippedWeapon;
    }
}

void InventoryState::Close() {
    m_game->SetState(std::make_unique<PlayingState>(m_game));
}

void InventoryState::HandleInput() {
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_I)) { Close(); return; }

    Player* player = m_game->GetPlayer();
    if (!player) return;
    core::Character& c = player->GetCharacterMutable();
    const int n = static_cast<int>(c.ownedWeapons.size());
    if (n == 0) return;

    if (IsKeyPressed(KEY_DOWN)) m_selected = (m_selected + 1) % n;
    if (IsKeyPressed(KEY_UP))   m_selected = (m_selected - 1 + n) % n;

    if (IsKeyPressed(KEY_ENTER)) {
        // Se è già equipaggiata la rimette in inventario (disarmato)
        c.equippedWeapon = (c.equippedWeapon == m_selected) ? -1 : m_selected;
    }
}

void InventoryState::Render() {
    const Player* player = m_game->GetPlayer();
    if (!player) return;
    const core::Character& c = player->GetCharacter();

    const int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.6f));

    const Rectangle b = { (sw - 460.0f) / 2, (sh - 420.0f) / 2, 460.0f, 420.0f };
    DrawRectangleRec(b, kWood);
    DrawRectangleLinesEx(b, 3.0f, kGoldDim);

    const Font& font = UIFont();
    DrawTextCentered(font, "Inventario", b.x + b.width / 2, b.y + 18, 30.0f, kGoldBright);
    DrawTextEx(font, "Armi", { b.x + 30, b.y + 70 }, 20.0f, 2.0f, Fade(kBone, 0.7f));

    float y = b.y + 100;
    for (int i = 0; i < (int)c.ownedWeapons.size(); ++i) {
        const auto& w = c.ownedWeapons[i];
        const Rectangle row = { b.x + 20, y - 4, b.width - 40, 34 };
        if (i == m_selected) {
            DrawRectangleRec(row, Fade(kGold, 0.14f));
            DrawRectangleLinesEx(row, 1.0f, Fade(kGold, 0.7f));
        }
        const bool eq = (c.equippedWeapon == i);
        const char* txt = TextFormat("%s  %s  (portata %.1fm)%s",
                                     w.name, core::ToString(w.damage).c_str(), w.range,
                                     eq ? "   [EQUIPAGGIATA]" : "");
        DrawTextEx(font, txt, { b.x + 30, y }, 20.0f, 2.0f, eq ? kGoldBright : kBone);
        y += 38;
    }

    DrawTextCentered(font, "SU/GIU: seleziona   INVIO: equipaggia / rimuovi   I/ESC: chiudi",
                     b.x + b.width / 2, b.y + b.height - 34, 16.0f, Fade(kBone, 0.7f));
}