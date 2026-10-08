#include "states/InventoryState.h"
#include "states/PlayingState.h"
#include "states/VillageInteriorState.h"
#include "core/Game.h"
#include "core/KeyBindings.h"
#include "core/Localization.h"
#include "entities/Player.h"
#include "ui/GothicTheme.h"
#include "rendering/LightingSystem.h"
#include "rendering/ItemVisuals.h"
#include "world/Village.h"
#include <algorithm>
#include <raylib.h>
#include <raymath.h>
#include <memory>
#include <iostream>

using namespace ui::theme;

InventoryState::InventoryState(Game* game, InteriorChest* chest)
    : GameState(game), m_containerChest(chest) {
    m_type = GameStateType::Inventory;
    m_isDragging = false;
    m_draggedItemId = core::kNoItem;
    m_draggedItemCount = 0;
    m_dragSource = DRAG_NONE;
    m_dragSourceIndex = -1;
    m_scrollOffset = 0;
    m_maxScroll = 0;
}

InventoryState::~InventoryState() {
}

void InventoryState::Enter() {
    m_previewTexture = LoadRenderTexture(256, 384);
}

void InventoryState::Exit() {
    UnloadRenderTexture(m_previewTexture);
}

void InventoryState::HandleInput() {
    if (core::KeyBindings::Pressed(core::KeyBindings::Action::Back) ||
        core::KeyBindings::Pressed(core::KeyBindings::Action::Inventory)) {
        if (m_isDragging) {
            m_game->GetPlayer()->GetCharacterMutable().AddItem(m_draggedItemId, m_draggedItemCount);
            auto& inv = m_game->GetPlayer()->GetCharacterMutable().inventory;
        inv.erase(std::remove_if(inv.begin(), inv.end(),
                  [](const core::ItemStack& s) { return s.id == core::kNoItem || s.count <= 0; }), inv.end());
            m_isDragging = false;
        }
        if (m_game->GetActiveBuilding() >= 0) {
            m_game->SetState(std::make_unique<VillageInteriorState>(m_game, m_game->GetActiveBuilding()));
        } else {
            m_game->SetState(std::make_unique<PlayingState>(m_game));
        }
        return; 
    }

    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
        m_scrollOffset -= (int)wheel;
        if (m_scrollOffset < 0) m_scrollOffset = 0;
        if (m_scrollOffset > m_maxScroll) m_scrollOffset = m_maxScroll;
    }
}

void InventoryState::Update(float deltaTime) {
    Player* player = m_game->GetPlayer();
    if (!player) return;
    
    core::Character& c = player->GetCharacterMutable();
    Vector2 mousePos = GetMousePosition();
    
    const int sw = GetScreenWidth(), sh = GetScreenHeight();
    const float panelW = m_containerChest ? 1020.0f : 800.0f, panelH = 500.0f;
    const float px = (sw - panelW) / 2.0f;
    const float py = (sh - panelH) / 2.0f;
    
    const float gridX = px + 450.0f;
    const float gridY = py + 60.0f;

    const float chestGridX = px + 740.0f;
    const float chestGridY = py + 100.0f;
    constexpr int CHEST_COLS = 4;
    constexpr int CHEST_ROWS = 4;

    // "Prendi Tutto" (Take All) button check
    if (m_containerChest && !m_isDragging && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Rectangle takeAllBtn = { px + 740.0f, py + 380.0f, 200.0f, 36.0f };
        if (CheckCollisionPointRec(mousePos, takeAllBtn)) {
            c.gold += m_containerChest->gold;
            m_containerChest->gold = 0;
            for (const auto& stack : m_containerChest->items) {
                if (stack.id != core::kNoItem && stack.count > 0) {
                    c.AddItem(stack.id, stack.count);
                }
            }
            m_containerChest->items.clear();
            m_containerChest->looted = true;
        }
    }
    
    int totalItems = (int)c.inventory.size();
    int totalRows = (totalItems + GRID_COLS - 1) / GRID_COLS;
    m_maxScroll = std::max(0, totalRows - GRID_ROWS);
    if (m_scrollOffset > m_maxScroll) m_scrollOffset = m_maxScroll;
    
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !m_isDragging) {
        for (int row = 0; row < GRID_ROWS; ++row) {
            for (int col = 0; col < GRID_COLS; ++col) {
                int itemIndex = (m_scrollOffset + row) * GRID_COLS + col;
                if (itemIndex < totalItems) {
                    float x = gridX + col * (SLOT_SIZE + SLOT_PADDING);
                    float y = gridY + row * (SLOT_SIZE + SLOT_PADDING);
                    Rectangle slotRec = { x, y, SLOT_SIZE, SLOT_SIZE };
                    
                    if (CheckCollisionPointRec(mousePos, slotRec)) {
                        m_isDragging = true;
                        m_draggedItemId = c.inventory[itemIndex].id;
                        m_draggedItemCount = c.inventory[itemIndex].count;
                        m_dragSource = DRAG_GRID;
                        m_dragSourceIndex = itemIndex;
                        
                        c.inventory[itemIndex].id = core::kNoItem;
                        c.inventory[itemIndex].count = 0;
                        break;
                    }
                }
            }
        }

        // Check chest slots drag start
        if (m_containerChest && !m_isDragging) {
            int chestTotalItems = (int)m_containerChest->items.size();
            for (int row = 0; row < CHEST_ROWS; ++row) {
                for (int col = 0; col < CHEST_COLS; ++col) {
                    int itemIndex = row * CHEST_COLS + col;
                    if (itemIndex < chestTotalItems) {
                        float x = chestGridX + col * (SLOT_SIZE + SLOT_PADDING);
                        float y = chestGridY + row * (SLOT_SIZE + SLOT_PADDING);
                        Rectangle slotRec = { x, y, SLOT_SIZE, SLOT_SIZE };

                        if (CheckCollisionPointRec(mousePos, slotRec)) {
                            m_isDragging = true;
                            m_draggedItemId = m_containerChest->items[itemIndex].id;
                            m_draggedItemCount = m_containerChest->items[itemIndex].count;
                            m_dragSource = DRAG_CHEST_GRID;
                            m_dragSourceIndex = itemIndex;

                            m_containerChest->items.erase(m_containerChest->items.begin() + itemIndex);
                            break;
                        }
                    }
                }
                if (m_isDragging) break;
            }
        }
        
        struct SlotDef { DragSource src; float x; float y; core::ItemId* idPtr; core::EquipSlot eSlot; };
        SlotDef equipSlots[] = {
            { DRAG_EQUIP_HEAD, px + 50, py + 80, &c.equipment.head, core::EquipSlot::Head },
            { DRAG_EQUIP_BODY, px + 50, py + 160, &c.equipment.body, core::EquipSlot::Body },
            { DRAG_EQUIP_LEGS, px + 50, py + 240, &c.equipment.legs, core::EquipSlot::Legs },
            { DRAG_EQUIP_FEET, px + 50, py + 320, &c.equipment.feet, core::EquipSlot::Feet },
            { DRAG_EQUIP_RIGHT, px + 350, py + 160, &c.equipment.rightHand, core::EquipSlot::RightHand },
            { DRAG_EQUIP_LEFT, px + 350, py + 240, &c.equipment.leftHand, core::EquipSlot::LeftHand }
        };
        
        for (const auto& s : equipSlots) {
            Rectangle r = { s.x, s.y, SLOT_SIZE, SLOT_SIZE };
            if (CheckCollisionPointRec(mousePos, r) && *s.idPtr != core::kNoItem) {
                m_isDragging = true;
                m_draggedItemId = *s.idPtr;
                m_draggedItemCount = 1;
                m_dragSource = s.src;
                *s.idPtr = core::kNoItem;
                break;
            }
        }

        const Rectangle quiverRect = { px + 350, py + 320, SLOT_SIZE, SLOT_SIZE };
        if (!m_isDragging && CheckCollisionPointRec(mousePos, quiverRect) &&
            c.equipment.quiver.id != core::kNoItem) {
            m_isDragging = true;
            m_draggedItemId = c.equipment.quiver.id;
            m_draggedItemCount = c.equipment.quiver.count;
            m_dragSource = DRAG_EQUIP_QUIVER;
            c.equipment.quiver = { core::kNoItem, 0 };
        }
    }
    else if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && m_isDragging) {
        bool dropped = false;

        // Check drop on chest grid
        if (m_containerChest) {
            Rectangle chestArea = { chestGridX - 10, chestGridY - 10, CHEST_COLS * (SLOT_SIZE + SLOT_PADDING) + 20, CHEST_ROWS * (SLOT_SIZE + SLOT_PADDING) + 20 };
            if (CheckCollisionPointRec(mousePos, chestArea)) {
                m_containerChest->items.push_back({ m_draggedItemId, m_draggedItemCount });
                dropped = true;
            }
        }
        
        struct SlotDef { DragSource src; float x; float y; core::ItemId* idPtr; core::EquipSlot eSlot; };
        SlotDef equipSlots[] = {
            { DRAG_EQUIP_HEAD, px + 50, py + 80, &c.equipment.head, core::EquipSlot::Head },
            { DRAG_EQUIP_BODY, px + 50, py + 160, &c.equipment.body, core::EquipSlot::Body },
            { DRAG_EQUIP_LEGS, px + 50, py + 240, &c.equipment.legs, core::EquipSlot::Legs },
            { DRAG_EQUIP_FEET, px + 50, py + 320, &c.equipment.feet, core::EquipSlot::Feet },
            { DRAG_EQUIP_RIGHT, px + 350, py + 160, &c.equipment.rightHand, core::EquipSlot::RightHand },
            { DRAG_EQUIP_LEFT, px + 350, py + 240, &c.equipment.leftHand, core::EquipSlot::LeftHand }
        };
        
        if (!dropped) {
            for (const auto& s : equipSlots) {
                Rectangle r = { s.x, s.y, SLOT_SIZE, SLOT_SIZE };
                if (CheckCollisionPointRec(mousePos, r)) {
                    if (c.CanEquip(m_draggedItemId, s.eSlot)) {
                        if (*s.idPtr != core::kNoItem) {
                            c.AddItem(*s.idPtr, 1);
                        }
                        *s.idPtr = m_draggedItemId;
                        dropped = true;
                    }
                    break;
                }
            }
        }

        const Rectangle quiverRect = { px + 350, py + 320, SLOT_SIZE, SLOT_SIZE };
        if (!dropped && CheckCollisionPointRec(mousePos, quiverRect)) {
            if (c.CanEquip(m_draggedItemId, core::EquipSlot::Quiver)) {
                if (c.equipment.quiver.id != core::kNoItem) {
                    c.AddItem(c.equipment.quiver.id, c.equipment.quiver.count);
                }
                c.equipment.quiver = { m_draggedItemId, m_draggedItemCount };
                dropped = true;
            }
        }
        
        if (!dropped) {
            for (int row = 0; row < GRID_ROWS; ++row) {
                for (int col = 0; col < GRID_COLS; ++col) {
                    float x = gridX + col * (SLOT_SIZE + SLOT_PADDING);
                    float y = gridY + row * (SLOT_SIZE + SLOT_PADDING);
                    Rectangle r = { x, y, SLOT_SIZE, SLOT_SIZE };
                    
                    if (CheckCollisionPointRec(mousePos, r)) {
                        int itemIndex = (m_scrollOffset + row) * GRID_COLS + col;
                        if (itemIndex < totalItems) {
                            if (c.inventory[itemIndex].id == core::kNoItem) {
                                c.inventory[itemIndex].id = m_draggedItemId;
                                c.inventory[itemIndex].count = m_draggedItemCount;
                            } else {
                                core::ItemId temp = c.inventory[itemIndex].id;
                                int tempCount = c.inventory[itemIndex].count;
                                c.inventory[itemIndex].id = m_draggedItemId;
                                c.inventory[itemIndex].count = m_draggedItemCount;
                                c.AddItem(temp, tempCount);
                            }
                        } else {
                            c.AddItem(m_draggedItemId, m_draggedItemCount);
                        }
                        dropped = true;
                        break;
                    }
                }
                if (dropped) break;
            }
        }
        
        if (!dropped) {
            if (m_dragSource == DRAG_CHEST_GRID && m_containerChest) {
                m_containerChest->items.push_back({ m_draggedItemId, m_draggedItemCount });
            } else {
                c.AddItem(m_draggedItemId, m_draggedItemCount);
            }
        }
        
        c.inventory.erase(
            std::remove_if(c.inventory.begin(), c.inventory.end(),
                           [](const core::ItemStack& stack) { return stack.id == core::kNoItem || stack.count <= 0; }),
            c.inventory.end());
            
        m_isDragging = false;
    }
}

void InventoryState::Render3DPreview(Rectangle bounds) {
    BeginTextureMode(m_previewTexture);
    ClearBackground(BLANK);
    
    Camera3D previewCamera = { 0 };
    previewCamera.position = Vector3{ 0.0f, 1.5f, 5.0f };
    previewCamera.target = Vector3{ 0.0f, 1.0f, 0.0f };
    previewCamera.up = Vector3{ 0.0f, 1.0f, 0.0f };
    previewCamera.fovy = 45.0f;
    previewCamera.projection = CAMERA_PERSPECTIVE;
    
    BeginMode3D(previewCamera);
    
    // No LightingSystem here — inventory preview must always be fully lit
    // regardless of in-game time of day.
    
    if (m_game->GetPlayer()) {
        Player* p = m_game->GetPlayer();
        Vector3 oldPos = p->GetPosition();
        p->SetPosition({0,0,0});
        p->Render();
        p->SetPosition(oldPos);
    }
    
    EndMode3D();
    EndTextureMode();
    
    Rectangle source = { 0.0f, 0.0f, (float)m_previewTexture.texture.width, -(float)m_previewTexture.texture.height };
    DrawTexturePro(m_previewTexture.texture, source, bounds, Vector2{0.0f, 0.0f}, 0.0f, WHITE);
}

void InventoryState::DrawSlot(Rectangle rect, core::ItemId id, int count, bool isHovered, const char* label, bool isInvalidDrop) {
    Color bg = isInvalidDrop ? Fade(RED, 0.4f) : (isHovered ? Fade(kGold, 0.3f) : Fade(BLACK, 0.6f));
    Color border = isInvalidDrop ? RED : (isHovered ? kGoldBright : kGoldDim);
    DrawRectangleRec(rect, bg);
    DrawRectangleLinesEx(rect, 2.0f, border);
    
    if (label) {
        const std::string localizedLabel = lang::Get(label);
        DrawText(localizedLabel.c_str(), (int)rect.x, (int)(rect.y - 14), 10, kGoldDim);
    }
    
     if (id != core::kNoItem) {
        ItemVisuals* visuals = m_game ? m_game->GetItemVisuals() : nullptr;
        if (visuals) {
            visuals->DrawIcon(id, { rect.x + 4, rect.y + 4, rect.width - 8, rect.height - 8 });
        } else if (const core::ItemDef* def = core::GetItem(id)) {
            const std::string name = lang::ItemName(def->key);
            DrawText(TextSubtext(name.c_str(), 0, 4), (int)rect.x + 4, (int)rect.y + 16, 10, kBone);
        }
        if (count > 1) DrawText(TextFormat("x%d", count), (int)rect.x + 2, (int)(rect.y + SLOT_SIZE - 12), 10, WHITE);
    }
}

void InventoryState::Render() {
    const int sw = GetScreenWidth(), sh = GetScreenHeight();
    const Font& font = UIFont();
    Vector2 mousePos = GetMousePosition();

    Player* p = m_game->GetPlayer();
    if (!p) return;
    const core::Character& c = p->GetCharacter();

    // ─────────────────────────────────────────────────────────────
    // CHEST MODE — full-screen opaque, two panels side-by-side
    // ─────────────────────────────────────────────────────────────
    if (m_containerChest) {
        // Solid background — world completely hidden
        DrawRectangle(0, 0, sw, sh, Color{ 18, 14, 10, 255 });

        constexpr int CHEST_COLS = 4;
        constexpr int CHEST_ROWS = 4;

        // ── Left panel: backpack ─────────────────────────────────
        const float leftW = 420.0f, panelH = 520.0f;
        const float leftX = sw / 2.0f - leftW - 20.0f;
        const float topY  = (sh - panelH) / 2.0f;

        Rectangle leftPanel = { leftX, topY, leftW, panelH };
        DrawRectangleRec(leftPanel, kIron);
        DrawRectangleLinesEx(leftPanel, 4.0f, kGoldDim);
        DrawRectangleLinesEx({ leftX+6, topY+6, leftW-12, panelH-12 }, 1.0f, kGold);

        DrawTextEx(font, lang::Get("inventory.backpack").c_str(), { leftX + 20, topY + 16 }, 24.0f, 1.5f, kGoldBright);

        // Equipment slots (compact column on the left side of the panel)
        struct SlotDef { float x; float y; core::ItemId id; const char* label; core::EquipSlot eSlot; };
        SlotDef equipSlots[] = {
            { leftX + 20, topY + 60,  c.equipment.head,      "inventory.head",       core::EquipSlot::Head },
            { leftX + 20, topY + 130, c.equipment.body,      "inventory.body",       core::EquipSlot::Body },
            { leftX + 20, topY + 200, c.equipment.legs,      "inventory.legs",       core::EquipSlot::Legs },
            { leftX + 20, topY + 270, c.equipment.feet,      "inventory.feet",       core::EquipSlot::Feet },
            { leftX + 20, topY + 340, c.equipment.rightHand, "inventory.right_hand", core::EquipSlot::RightHand },
            { leftX + 20, topY + 410, c.equipment.leftHand,  "inventory.left_hand",  core::EquipSlot::LeftHand },
        };
        for (const auto& s : equipSlots) {
            Rectangle r = { s.x, s.y, SLOT_SIZE, SLOT_SIZE };
            bool hovered    = !m_isDragging && CheckCollisionPointRec(mousePos, r);
            bool invalid    = m_isDragging  && CheckCollisionPointRec(mousePos, r) && !c.CanEquip(m_draggedItemId, s.eSlot);
            DrawSlot(r, s.id, 1, hovered, s.label, invalid);
        }

        // Backpack grid
        const float bgridX = leftX + 120.0f;
        const float bgridY = topY + 60.0f;
        int totalItems = (int)c.inventory.size();
        for (int row = 0; row < GRID_ROWS; ++row) {
            for (int col = 0; col < GRID_COLS; ++col) {
                float x = bgridX + col * (SLOT_SIZE + SLOT_PADDING);
                float y = bgridY + row * (SLOT_SIZE + SLOT_PADDING);
                Rectangle r = { x, y, SLOT_SIZE, SLOT_SIZE };
                int itemIndex = (m_scrollOffset + row) * GRID_COLS + col;
                bool hovered = !m_isDragging && CheckCollisionPointRec(mousePos, r);
                if (itemIndex < totalItems)
                    DrawSlot(r, c.inventory[itemIndex].id, c.inventory[itemIndex].count, hovered);
                else
                    DrawSlot(r, core::kNoItem, 0, hovered);
            }
        }

        // ── Right panel: chest ───────────────────────────────────
        const float rightW = 320.0f;
        const float rightX = sw / 2.0f + 20.0f;

        Rectangle rightPanel = { rightX, topY, rightW, panelH };
        DrawRectangleRec(rightPanel, kIron);
        DrawRectangleLinesEx(rightPanel, 4.0f, kGoldDim);
        DrawRectangleLinesEx({ rightX+6, topY+6, rightW-12, panelH-12 }, 1.0f, kGold);

        DrawTextEx(font, "Cassa", { rightX + 20, topY + 16 }, 24.0f, 1.5f, kGoldBright);
        if (m_containerChest->gold > 0)
            DrawTextEx(font, TextFormat("%d Monete Oro", m_containerChest->gold), { rightX + 20, topY + 52 }, 16.0f, 1.0f, kBone);

        const float chestGridX = rightX + 20.0f;
        const float chestGridY = topY + 90.0f;
        int chestTotalItems = (int)m_containerChest->items.size();
        for (int row = 0; row < CHEST_ROWS; ++row) {
            for (int col = 0; col < CHEST_COLS; ++col) {
                float x = chestGridX + col * (SLOT_SIZE + SLOT_PADDING);
                float y = chestGridY + row * (SLOT_SIZE + SLOT_PADDING);
                Rectangle r = { x, y, SLOT_SIZE, SLOT_SIZE };
                int itemIndex = row * CHEST_COLS + col;
                bool hovered = !m_isDragging && CheckCollisionPointRec(mousePos, r);
                if (itemIndex < chestTotalItems)
                    DrawSlot(r, m_containerChest->items[itemIndex].id, m_containerChest->items[itemIndex].count, hovered);
                else
                    DrawSlot(r, core::kNoItem, 0, hovered);
            }
        }

        // "Prendi Tutto" button
        Rectangle takeAllBtn = { rightX + 20.0f, topY + panelH - 70.0f, 200.0f, 40.0f };
        bool btnHovered = CheckCollisionPointRec(mousePos, takeAllBtn);
        DrawRectangleRec(takeAllBtn, btnHovered ? Fade(kGold, 0.4f) : Fade(BLACK, 0.6f));
        DrawRectangleLinesEx(takeAllBtn, 2.0f, btnHovered ? kGoldBright : kGoldDim);
        DrawTextEx(font, lang::Get("inventory.take_all").c_str(), { takeAllBtn.x + 20.0f, takeAllBtn.y + 10.0f }, 18.0f, 1.0f, kGoldBright);

        // Close hint
        DrawTextEx(font, "[ESC] Chiudi", { (float)sw/2 - 60, topY + panelH + 12 }, 16.0f, 1.0f, Fade(kBone, 0.7f));

        // Dragged item on cursor
        if (m_isDragging) {
            Rectangle dragRect = { mousePos.x - SLOT_SIZE/2, mousePos.y - SLOT_SIZE/2, SLOT_SIZE, SLOT_SIZE };
            DrawSlot(dragRect, m_draggedItemId, m_draggedItemCount, false);
        }
        return;
    }

    // ─────────────────────────────────────────────────────────────
    // NORMAL INVENTORY MODE
    // ─────────────────────────────────────────────────────────────
    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.7f));
    
    const float panelW = 800.0f, panelH = 500.0f;
    const float px = (sw - panelW) / 2.0f;
    const float py = (sh - panelH) / 2.0f;
    const Rectangle panelBounds = { px, py, panelW, panelH };
    
    DrawRectangleRec(panelBounds, kIron);
    DrawRectangleLinesEx(panelBounds, 4.0f, kGoldDim);
    DrawRectangleLinesEx({px+6, py+6, panelW-12, panelH-12}, 1.0f, kGold);
    
    DrawTextEx(font, lang::Get("inventory.title").c_str(), { px + 24, py + 16 }, 28.0f, 2.0f, kGoldBright);
    
    Rectangle previewBounds = { px + 120, py + 80, 200, 350 };
    DrawRectangleRec(previewBounds, Fade(BLACK, 0.4f));
    Render3DPreview(previewBounds);
    DrawRectangleLinesEx(previewBounds, 2.0f, kGoldDim);
    
    struct SlotDef { float x; float y; core::ItemId id; const char* label; core::EquipSlot eSlot; };
    SlotDef equipSlots[] = {
        { px + 50, py + 80,  c.equipment.head,      "inventory.head",       core::EquipSlot::Head },
        { px + 50, py + 160, c.equipment.body,       "inventory.body",       core::EquipSlot::Body },
        { px + 50, py + 240, c.equipment.legs,       "inventory.legs",       core::EquipSlot::Legs },
        { px + 50, py + 320, c.equipment.feet,       "inventory.feet",       core::EquipSlot::Feet },
        { px + 350, py + 160, c.equipment.rightHand, "inventory.right_hand", core::EquipSlot::RightHand },
        { px + 350, py + 240, c.equipment.leftHand,  "inventory.left_hand",  core::EquipSlot::LeftHand }
    };
    
    for (const auto& s : equipSlots) {
        Rectangle r = { s.x, s.y, SLOT_SIZE, SLOT_SIZE };
        bool hovered = !m_isDragging && CheckCollisionPointRec(mousePos, r);
        bool dropHovered = m_isDragging && CheckCollisionPointRec(mousePos, r);
        bool isInvalidDrop = dropHovered && !c.CanEquip(m_draggedItemId, s.eSlot);
        DrawSlot(r, s.id, 1, hovered, s.label, isInvalidDrop);
    }

    const Rectangle quiverRect = { px + 350, py + 320, SLOT_SIZE, SLOT_SIZE };
    const bool quiverHovered = !m_isDragging && CheckCollisionPointRec(mousePos, quiverRect);
    const bool quiverInvalidDrop = m_isDragging && CheckCollisionPointRec(mousePos, quiverRect) &&
                                   !c.CanEquip(m_draggedItemId, core::EquipSlot::Quiver);
    DrawSlot(quiverRect, c.equipment.quiver.id, c.equipment.quiver.count, quiverHovered,
             "inventory.quiver", quiverInvalidDrop);
    
    const float gridX = px + 450.0f;
    const float gridY = py + 60.0f;
    int totalItems = (int)c.inventory.size();
    
    DrawTextEx(font, lang::Get("inventory.backpack").c_str(), { gridX, gridY - 24 }, 20.0f, 1.0f, kBone);
    
    for (int row = 0; row < GRID_ROWS; ++row) {
        for (int col = 0; col < GRID_COLS; ++col) {
            float x = gridX + col * (SLOT_SIZE + SLOT_PADDING);
            float y = gridY + row * (SLOT_SIZE + SLOT_PADDING);
            Rectangle r = { x, y, SLOT_SIZE, SLOT_SIZE };
            
            int itemIndex = (m_scrollOffset + row) * GRID_COLS + col;
            bool hovered = !m_isDragging && CheckCollisionPointRec(mousePos, r);
            
            if (itemIndex < totalItems) {
                DrawSlot(r, c.inventory[itemIndex].id, c.inventory[itemIndex].count, hovered);
            } else {
                DrawSlot(r, core::kNoItem, 0, hovered);
            }
        }
    }
    
    float scrollBarX = gridX + GRID_COLS * (SLOT_SIZE + SLOT_PADDING) + 10;
    float scrollBarH = GRID_ROWS * (SLOT_SIZE + SLOT_PADDING);
    DrawRectangle((int)scrollBarX, (int)gridY, 10, (int)scrollBarH, Fade(BLACK, 0.5f));
    
    if (m_maxScroll > 0) {
        float thumbH = scrollBarH * ((float)GRID_ROWS / (m_maxScroll + GRID_ROWS));
        float thumbY = gridY + (scrollBarH - thumbH) * ((float)m_scrollOffset / m_maxScroll);
        DrawRectangle((int)scrollBarX, (int)thumbY, 10, (int)thumbH, kGoldDim);
    }
    
    if (m_isDragging) {
        Rectangle dragRect = { mousePos.x - SLOT_SIZE/2, mousePos.y - SLOT_SIZE/2, SLOT_SIZE, SLOT_SIZE };
        DrawSlot(dragRect, m_draggedItemId, m_draggedItemCount, false);
    }
}

