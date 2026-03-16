// src/Guis/Skills/SkillsPanel.h
//
// Phase 5 — Client-side skills/XP panel.
//
// SkillsPanel is an ImGui window that shows every skill as a block containing:
//   • Skill icon placeholder (coloured square until texture atlas is wired up)
//   • Current level (computed from raw XP via the OSRS formula)
//   • Skill name
//
// Hovering a skill block for ≥ 0.2 s shows a tooltip with the exact XP
// remaining until the next level.
//
// The panel subscribes to SkillsSyncReceivedEvent so it refreshes automatically
// whenever a SkillsSyncPacket arrives from the server.

#ifndef ENGINE_SKILLSPANEL_H
#define ENGINE_SKILLSPANEL_H

#include "../../Network/NetworkPackets.h"
#include "../../ECS/Components/SkillsComponent.h"
#include <array>

class SkillsPanel {
public:
    /// Singleton accessor.
    static SkillsPanel& instance();

    // ------------------------------------------------------------------
    // Data update (called by NetworkSystem on SkillsSyncPacket arrival)
    // ------------------------------------------------------------------
    void applySync(const Network::SkillsSyncPacket& pkt);

    // ------------------------------------------------------------------
    // Per-frame rendering
    // ------------------------------------------------------------------
    void render();

    void show()   { visible_ = true;  }
    void hide()   { visible_ = false; }
    void toggle() { visible_ = !visible_; }
    bool isVisible() const { return visible_; }

private:
    SkillsPanel() = default;

    static constexpr int kSkills = Network::kSkillCount;
    static constexpr int kCols   = 3;

    bool visible_ = false;

    std::array<uint32_t, kSkills> xp_{};

    /// Per-skill hover accumulator (seconds).
    std::array<float, kSkills> hoverTime_{};
    static constexpr float kTooltipDelay = 0.2f;
};

#endif // ENGINE_SKILLSPANEL_H
