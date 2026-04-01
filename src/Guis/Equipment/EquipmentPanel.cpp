// src/Guis/Equipment/EquipmentPanel.cpp

#include "EquipmentPanel.h"
#include "../../ECS/Components/AnimatedModelComponent.h"
#include "../../Animation/EquipmentSlot.h"
#include "../UiMaster.h"

#include <imgui.h>
#include <iostream>

EquipmentPanel& EquipmentPanel::instance() {
    static EquipmentPanel inst;
    return inst;
}

void EquipmentPanel::render() {
    if (!visible_ || !registry_) return;

    // Find the local player's AnimatedModelComponent.
    AnimatedModelComponent* playerAmc = nullptr;
    auto view = registry_->view<AnimatedModelComponent>();
    for (auto entity : view) {
        auto& amc = view.get<AnimatedModelComponent>(entity);
        if (amc.isLocalPlayer && amc.isModular) {
            playerAmc = &amc;
            break;
        }
    }

    if (!playerAmc) return;

    ImGui::SetNextWindowSize(ImVec2(260, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(10, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Equipment", &visible_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    // Register the window rectangle so InputDispatcher knows not to pass
    // clicks through to the 3D world.
    ImVec2 wPos  = ImGui::GetWindowPos();
    ImVec2 wSize = ImGui::GetWindowSize();
    UiMaster::registerUiRegion(wPos.x, wPos.y, wSize.x, wSize.y);

    ImGui::TextUnformatted("Toggle armor pieces:");
    ImGui::Separator();

    // Iterate over all equipment slots.
    for (int i = 0; i < static_cast<int>(EquipmentSlot::Count); ++i) {
        auto slot = static_cast<EquipmentSlot>(i);
        const char* slotName = equipmentSlotToString(slot);

        // Only show slots that have a default equipment path (i.e. the prefab
        // defined default_equipment for this slot).
        auto it = playerAmc->defaultEquipmentPaths.find(i);
        if (it == playerAmc->defaultEquipmentPaths.end()) continue;

        bool equipped = (playerAmc->equippedArmor[i] != nullptr);

        if (ImGui::Checkbox(slotName, &equipped)) {
            if (equipped) {
                // Re-equip the default piece.
                playerAmc->equipPart(slot, it->second);
                std::cout << "[EquipmentPanel] Equipped " << slotName
                          << " from default path.\n";
            } else {
                // Unequip — naked geometry becomes visible.
                playerAmc->unequipPart(slot);
                std::cout << "[EquipmentPanel] Unequipped " << slotName << ".\n";
            }
            // Reset the one-shot log so the next buildActiveMeshes prints a summary.
            playerAmc->activeMeshesLoggedOnce_ = false;
        }
    }

    ImGui::End();
}
