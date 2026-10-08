#include "states/InventoryState.h"
#include "states/PlayingState.h"
#include "core/Game.h"
#include "core/KeyBindings.h"
#include "core/Localization.h"
#include "entities/Player.h"
#include "ui/GothicTheme.h"
#include "rendering/LightingSystem.h"
#include "rendering/ItemVisuals.h"
#include <algorithm>
#include <raylib.h>
#include <raymath.h>
#include <memory>
#include <iostream>

using namespace ui::theme;

InventoryState::InventoryState(Game* game) : GameState(game) {
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
        m_game->SetState(std::make_unique<PlayingState>(m_game));
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
    const float panelW = 800.0f, panelH = 500.0f;
    const float px = (sw - panelW) / 2.0f;
    const float py = (sh - panelH) / 2.0f;
    
    const float gridX = px + 450.0f;
    const float gridY = py + 60.0f;
    
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
            c.AddItem(m_draggedItemId, m_draggedItemCount);
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
    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.7f));
    
    const float panelW = 800.0f, panelH = 500.0f;
    const float px = (sw - panelW) / 2.0f;
    const float py = (sh - panelH) / 2.0f;
    const Rectangle panelBounds = { px, py, panelW, panelH };
    
    DrawRectangleRec(panelBounds, kIron);
    DrawRectangleLinesEx(panelBounds, 4.0f, kGoldDim);
    DrawRectangleLinesEx({px+6, py+6, panelW-12, panelH-12}, 1.0f, kGold);
    
    const Font& font = UIFont();
    DrawTextEx(font, lang::Get("inventory.title").c_str(), { px + 24, py + 16 }, 28.0f, 2.0f, kGoldBright);
    
    Player* p = m_game->GetPlayer();
    if (!p) return;
    const core::Character& c = p->GetCharacter();
    Vector2 mousePos = GetMousePosition();
    
    Rectangle previewBounds = { px + 120, py + 80, 200, 350 };
    DrawRectangleRec(previewBounds, Fade(BLACK, 0.4f));
    Render3DPreview(previewBounds);
    DrawRectangleLinesEx(previewBounds, 2.0f, kGoldDim);
    
    struct SlotDef { float x; float y; core::ItemId id; const char* label; core::EquipSlot eSlot; };
    SlotDef equipSlots[] = {
        { px + 50, py + 80, c.equipment.head, "inventory.head", core::EquipSlot::Head },
        { px + 50, py + 160, c.equipment.body, "inventory.body", core::EquipSlot::Body },
        { px + 50, py + 240, c.equipment.legs, "inventory.legs", core::EquipSlot::Legs },
        { px + 50, py + 320, c.equipment.feet, "inventory.feet", core::EquipSlot::Feet },
        { px + 350, py + 160, c.equipment.rightHand, "inventory.right_hand", core::EquipSlot::RightHand },
        { px + 350, py + 240, c.equipment.leftHand, "inventory.left_hand", core::EquipSlot::LeftHand }
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
