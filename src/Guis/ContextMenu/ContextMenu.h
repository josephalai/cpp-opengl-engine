// src/Guis/ContextMenu/ContextMenu.h
//
// Phase 2 — OSRS-style right-click context menu.
//
// When the player right-clicks a world entity, InputDispatcher calls
// ContextMenu::show() with the entity's network ID, model alias, and the list
// of available actions read from the prefab JSON.  Each frame the context menu
// renders itself as an ImGui popup window positioned at the original click
// coordinates.  Selecting an option publishes a ContextMenuActionEvent via the
// EventBus; the NetworkSystem then forwards an ActionRequestPacket to the server.
//
// Clicking anywhere outside the menu (or pressing Escape) dismisses it.

#ifndef ENGINE_CONTEXTMENU_H
#define ENGINE_CONTEXTMENU_H

#include <string>
#include <vector>
#include <cstdint>

struct ContextMenuAction {
    int         id;     ///< ActionType int value (maps to Network::ActionType).
    std::string label;  ///< Human-readable option text, e.g. "Chop down".
};

class ContextMenu {
public:
    /// Singleton accessor — one global context menu.
    static ContextMenu& instance();

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    /// Show the context menu at screen position (screenX, screenY) for
    /// the entity identified by @p networkId with model alias @p modelType.
    /// @p actions is the list of options read from the prefab JSON.
    void show(float screenX, float screenY,
              uint32_t networkId,
              const std::string& modelType,
              const std::vector<ContextMenuAction>& actions);

    /// Hide / dismiss the menu without selecting anything.
    void dismiss();

    /// Returns true while the context menu is visible.
    bool isVisible() const { return visible_; }

    /// Returns true when the cursor is within the menu's bounding rectangle.
    /// Used by InputDispatcher to consume clicks and prevent world propagation.
    bool isMouseOver() const;

    // ------------------------------------------------------------------
    // Per-frame rendering (call from UISystem::update)
    // ------------------------------------------------------------------

    /// Draw the context menu via ImGui.  Must be called each frame between
    /// ImGui::NewFrame() and ImGui::Render().
    void render();

private:
    ContextMenu() = default;

    bool                          visible_     = false;
    float                         spawnX_      = 0.0f;
    float                         spawnY_      = 0.0f;
    uint32_t                      networkId_   = 0;
    std::string                   modelType_;
    std::vector<ContextMenuAction> actions_;

    /// Approximate menu bounds (updated each render frame).
    float menuW_ = 160.0f;
    float menuH_ = 0.0f;
};

#endif // ENGINE_CONTEXTMENU_H
