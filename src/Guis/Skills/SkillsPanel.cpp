// src/Guis/Skills/SkillsPanel.cpp

#include "SkillsPanel.h"
#include "../../Events/EventBus.h"
#include "../../Events/Event.h"

#include <imgui.h>
#include <string>
#include <iostream>

SkillsPanel& SkillsPanel::instance() {
    static SkillsPanel inst;
    return inst;
}

void SkillsPanel::init() {
    // Subscribe to skills sync events published by NetworkSystem when a
    // SkillsSyncPacket arrives from the server.  This decouples the UI from
    // the networking layer: NetworkSystem owns the packet; SkillsPanel owns
    // the visual representation.
    EventBus::instance().subscribe<SkillsSyncEvent>([this](const SkillsSyncEvent& e) {
        // Convert to the canonical packet layout so all XP-setting logic
        // lives in one place (applySync).
        Network::SkillsSyncPacket pkt{};
        for (int i = 0; i < kSkills; ++i) {
            pkt.xp[i] = e.xp[i];
        }
        applySync(pkt);
        if (!visible_) show();
    });
}

void SkillsPanel::applySync(const Network::SkillsSyncPacket& pkt) {
    for (int i = 0; i < kSkills; ++i) {
        xp_[i] = pkt.xp[i];
    }
    std::cout << "[SkillsPanel] Sync applied.\n";
}

void SkillsPanel::render() {
    if (!visible_) return;

    const float blockW  = 80.0f;
    const float blockH  = 28.0f;
    const float padding = 3.0f;

    const int rows      = (kSkills + kCols - 1) / kCols;
    const float totalW  = kCols * (blockW + padding) + padding + 16.0f;
    const float totalH  = rows  * (blockH + padding) + padding + 30.0f;

    ImGui::SetNextWindowSize(ImVec2(totalW, totalH), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.88f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar;
    if (!ImGui::Begin("Skills", &visible_, flags)) {
        ImGui::End();
        return;
    }

    ImGuiIO&    io       = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2      origin   = ImGui::GetCursorScreenPos();

    for (int i = 0; i < kSkills; ++i) {
        int row = i / kCols;
        int col = i % kCols;

        float x = origin.x + col * (blockW + padding);
        float y = origin.y + row * (blockH + padding);

        ImVec2 tl(x, y);
        ImVec2 br(x + blockW, y + blockH);

        // Background.
        drawList->AddRectFilled(tl, br, IM_COL32(30, 30, 30, 220), 3.0f);
        drawList->AddRect       (tl, br, IM_COL32(80, 80, 80, 255), 3.0f);

        int      lvl  = levelFromXp(xp_[i]);
        uint32_t xpLeft = xpToNextLevel(xp_[i]);

        // Colour-coded level (yellow at 99, white otherwise).
        ImU32 lvlColor = (lvl >= 99)
            ? IM_COL32(255, 220, 0, 255)
            : IM_COL32(220, 220, 220, 255);

        char levelStr[8];
        snprintf(levelStr, sizeof(levelStr), "%d", lvl);
        ImVec2 lvlPos(x + 3.0f, y + 3.0f);
        drawList->AddText(lvlPos, lvlColor, levelStr);

        // Skill name.
        const char* name = skillName(i);
        // Truncate to 8 visible characters + null terminator so the label fits
        // within the block width; the buffer is 9 bytes to accommodate this exactly.
        char nameStr[9];
        snprintf(nameStr, sizeof(nameStr), "%.8s", name);
        float nameX = x + 3.0f + ImGui::CalcTextSize("999 ").x;
        ImVec2 namePos(nameX, y + 3.0f);
        drawList->AddText(namePos, IM_COL32(180, 180, 180, 255), nameStr);

        // Invisible button for hover detection.
        ImGui::SetCursorScreenPos(tl);
        std::string btnId = "##skill" + std::to_string(i);
        ImGui::InvisibleButton(btnId.c_str(), ImVec2(blockW, blockH));

        bool hovered = ImGui::IsItemHovered();
        if (hovered) {
            hoverTime_[i] += io.DeltaTime;
        } else {
            hoverTime_[i] = 0.0f;
        }

        // Show tooltip after the hover delay.
        if (hovered && hoverTime_[i] >= kTooltipDelay) {
            ImGui::BeginTooltip();
            ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.3f, 1.0f), "%s", name);
            ImGui::Separator();
            ImGui::Text("Level:  %d / 99", lvl);
            ImGui::Text("XP:     %u", xp_[i]);
            if (lvl < 99) {
                ImGui::Text("To next: %u xp", xpLeft);
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "MAX LEVEL");
            }
            ImGui::EndTooltip();
        }
    }

    // Advance cursor past the grid.
    ImGui::SetCursorScreenPos(
        ImVec2(origin.x,
               origin.y + rows * (blockH + padding) + padding));

    ImGui::End();
}
