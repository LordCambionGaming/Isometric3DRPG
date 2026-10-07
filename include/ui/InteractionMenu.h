#pragma once
#include <raylib.h>
#include <string>
#include <vector>
#include <functional>

namespace ui {

// A single action shown in the context menu
struct InteractionAction {
    std::string label;           // e.g. "Attack", "Interact", "Open"
    std::function<void()> callback; // what to do when selected
};

// Generic descriptor of an interactable world object
struct Interactable {
    std::string name;                     // displayed at the top of the popup
    Vector3     worldPosition;            // 3D position (head/origin of the entity)
    std::vector<InteractionAction> actions;
};

// -----------------------------------------------------------------------
// InteractionMenu
//   Draw a floating context-menu in screen space above any Interactable.
//   Usage:
//     1. Call Open(interactable) when the player right-clicks / selects something.
//     2. Call Update() every frame (handles hover/click).
//     3. Call Draw(camera) every frame AFTER EndMode3D() (so it's unaffected by shaders).
//     4. Call Close() or let the player press ESC / click outside to dismiss.
// -----------------------------------------------------------------------
class InteractionMenu {
public:
    InteractionMenu();

    // Opens the menu for the given interactable. Replaces any previous menu.
    void Open(const Interactable& target);
    void Close();

    bool IsOpen() const { return m_open; }

    // Process hover and click input. Returns true while menu is still open.
    bool Update();

    // Draw in 2D screen-space (call between BeginDrawing / EndDrawing, OUTSIDE mode3D).
    void Draw(const Camera3D& camera) const;

private:
    bool        m_open       = false;
    Interactable m_target;

    // Layout cache (rebuilt in Draw)
    static constexpr int   kItemH    = 36;
    static constexpr int   kPadX     = 14;
    static constexpr int   kPadY     = 8;
    static constexpr int   kMinWidth = 140;
    static constexpr float kCorner   = 6.0f;
    static constexpr int   kHeaderH  = 34;

    mutable int m_hoveredIndex = -1;
    mutable Rectangle m_bounds = {};
};

} // namespace ui
