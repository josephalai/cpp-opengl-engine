// src/Guis/ContextMenu/ContextMenu.cpp

#include "ContextMenu.h"
#include "../../Events/EventBus.h"
#include "../../Events/Event.h"
#include "../../Input/InputMaster.h"

#include <imgui.h>
#include <string>
#include <iostream>

ContextMenu& ContextMenu::instance() {
    static ContextMenu inst;
    return inst;
}

void ContextMenu::show(float screenX, float screenY,
                       uint32_t networkId,
                       const std::string& modelType,
                       const std::vector<ContextMenuAction>& actions) {
    visible_   = true;
    spawnX_    = screenX;
    spawnY_    = screenY;
    networkId_ = networkId;
    modelType_ = modelType;
    actions_   = actions;
    menuH_     = 0.0f; // will be measured on first render
    std::cout << "[ContextMenu] Showing for entity " << networkId
              << " (" << modelType << ") with "
              << actions.size() << " action(s) at ("
              << screenX << ", " << screenY << ")\n";
}

void ContextMenu::dismiss() {
    if (visible_) {
        visible_ = false;
        std::cout << "[ContextMenu] Dismissed.\n";
    }
}

bool ContextMenu::isMouseOver() const {
    if (!visible_) return false;
    double mx = InputMaster::mouseX;
    double my = InputMaster::mouseY;
    return (mx >= spawnX_ && mx <= spawnX_ + menuW_ &&
            my >= spawnY_ && my <= spawnY_ + menuH_);
}

void ContextMenu::render() {
    if (!visible_) return;

    // Position the popup exactly at the cursor's click coordinates.
    ImGui::SetNextWindowPos(ImVec2(spawnX_, spawnY_), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(menuW_, 0.0f), ImGuiCond_Always); // height = auto

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
                           | ImGuiWindowFlags_NoResize
                           | ImGuiWindowFlags_NoMove
                           | ImGuiWindowFlags_NoScrollbar
                           | ImGuiWindowFlags_AlwaysAutoResize
                           | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.08f, 0.92f));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.55f, 0.55f, 0.55f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(6.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,    ImVec2(4.0f, 3.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

    if (ImGui::Begin("##ContextMenu", nullptr, flags)) {
        // Header: show entity alias in a muted colour.
        ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.3f, 1.0f), "%s", modelType_.c_str());
        ImGui::Separator();

        for (std::size_t i = 0; i < actions_.size(); ++i) {
            const auto& act = actions_[i];
            std::string btnLabel = act.label + "##" + std::to_string(i);
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f,0.5f,1.0f,0.4f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.2f,0.5f,1.0f,0.7f));

            if (ImGui::Button(btnLabel.c_str(), ImVec2(-1, 0))) {
                // Publish the selection and close the menu.
                ContextMenuActionEvent evt{};
                evt.targetNetworkId = networkId_;
                evt.actionIndex     = static_cast<int>(i);
                evt.actionName      = act.label;
                EventBus::instance().publish(evt);
                std::cout << "[ContextMenu] Selected \"" << act.label
                          << "\" (action id=" << act.id
                          << ") on entity " << networkId_ << "\n";
                visible_ = false;
                ImGui::PopStyleColor(3);
                break;
            }
            ImGui::PopStyleColor(3);
        }

        // Record the actual rendered height for hit-testing.
        menuH_ = ImGui::GetWindowSize().y;
        menuW_ = ImGui::GetWindowSize().x;
    }
    ImGui::End();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);

    // Dismiss on click outside the menu.
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !isMouseOver()) {
        dismiss();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        dismiss();
    }
}
