#pragma once

#include "states/GameState.h"
#include "core/Items.h"
#include <raylib.h>

class InventoryState : public GameState {
public:
    explicit InventoryState(Game* game);
    ~InventoryState() override;

    void HandleInput() override;
    void Update(float deltaTime) override;
    void Render() override;
    void Enter() override;
    void Exit() override;

private:
    void Render3DPreview(Rectangle bounds);
    void DrawSlot(Rectangle rect, core::ItemId id, int count, bool isHovered, const char* label = nullptr);
    
    enum DragSource {
        DRAG_NONE,
        DRAG_GRID,
        DRAG_EQUIP_HEAD,
        DRAG_EQUIP_BODY,
        DRAG_EQUIP_LEGS,
        DRAG_EQUIP_FEET,
        DRAG_EQUIP_RIGHT,
        DRAG_EQUIP_LEFT
    };

    RenderTexture2D m_previewTexture;
    
    // Drag and drop state
    bool m_isDragging;
    core::ItemId m_draggedItemId;
    int m_draggedItemCount;
    DragSource m_dragSource;
    int m_dragSourceIndex;

    int m_scrollOffset;
    int m_maxScroll;
    
    // UI Layout constants
    static constexpr int GRID_COLS = 5;
    static constexpr int GRID_ROWS = 6;
    static constexpr float SLOT_SIZE = 48.0f;
    static constexpr float SLOT_PADDING = 4.0f;
};