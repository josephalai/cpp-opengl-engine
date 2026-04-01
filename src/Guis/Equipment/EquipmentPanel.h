// src/Guis/Equipment/EquipmentPanel.h
//
// Phase 6 — Equipment toggle panel.
//
// An ImGui panel that lets the player toggle individual armor pieces
// on and off for the local player's modular character.  Pressing the
// toggle key (default: G) shows/hides the panel.  Each equipment slot
// is shown as a checkbox; unchecking it calls unequipPart(), and
// re-checking it calls equipPart() with the stored default asset path.

#ifndef ENGINE_EQUIPMENTPANEL_H
#define ENGINE_EQUIPMENTPANEL_H

#include <entt/entt.hpp>

class EquipmentPanel {
public:
    /// Singleton accessor.
    static EquipmentPanel& instance();

    /// Bind the ECS registry so the panel can find the local player entity.
    void setRegistry(entt::registry* reg) { registry_ = reg; }

    /// Per-frame rendering (call inside an ImGui frame).
    void render();

    void show()   { visible_ = true;  }
    void hide()   { visible_ = false; }
    void toggle() { visible_ = !visible_; }
    bool isVisible() const { return visible_; }

private:
    EquipmentPanel() = default;

    entt::registry* registry_ = nullptr;
    bool visible_ = false;
};

#endif // ENGINE_EQUIPMENTPANEL_H
