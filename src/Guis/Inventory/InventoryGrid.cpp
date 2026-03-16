// src/Guis/Inventory/InventoryGrid.cpp

#include "InventoryGrid.h"
#include "../../Events/EventBus.h"
#include "../../Events/Event.h"
#include "../UiMaster.h"

#include <imgui.h>
#include <string>
#include <iostream>

InventoryGrid& InventoryGrid::instance() {
    static InventoryGrid inst;
    return inst;
}

void InventoryGrid::init() {
    // Subscribe to inventory sync events published by NetworkSystem when an
    // InventorySyncPacket arrives from the server.  This decouples the UI from
    // the networking layer: NetworkSystem owns the packet; InventoryGrid owns
    // the visual representation.
    EventBus::instance().subscribe<InventorySyncEvent>([this](const InventorySyncEvent& e) {
        // Convert to the canonical packet layout so all slot-setting logic
        // lives in one place (applySync).
        Network::InventorySyncPacket pkt{};
        for (int i = 0; i < kSlots; ++i) {
            pkt.itemIds[i]    = e.itemIds[i];
            pkt.quantities[i] = e.quantities[i];
        }
        applySync(pkt);
        if (!visible_) show();
    });
}

void InventoryGrid::applySync(const Network::InventorySyncPacket& pkt) {
    for (int i = 0; i < kSlots; ++i) {
        slots_[i].itemId   = pkt.itemIds[i];
        slots_[i].quantity = pkt.quantities[i];
    }
    std::cout << "[InventoryGrid] Sync applied.\n";
}

void InventoryGrid::render() {
    if (!visible_) return;

    // Reset per-frame drag tracking; set below if a drag is active.
    dragSrc_ = -1;

    const float padding   = 4.0f;
    const float totalW    = kCols * (kSlotSize + padding) + padding + 16.0f; // +16 for scroll bar
    const float totalH    = kRows * (kSlotSize + padding) + padding + 30.0f; // +30 for title

    ImGui::SetNextWindowSize(ImVec2(totalW, totalH), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.88f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar;
    if (!ImGui::Begin("Inventory", &visible_, flags)) {
        ImGui::End();
        return;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();

    for (int row = 0; row < kRows; ++row) {
        for (int col = 0; col < kCols; ++col) {
            int idx = row * kCols + col;
            float x  = origin.x + col * (kSlotSize + padding);
            float y  = origin.y + row * (kSlotSize + padding);
            ImVec2 tl(x, y);
            ImVec2 br(x + kSlotSize, y + kSlotSize);

            // Draw slot background.
            uint32_t bgCol = IM_COL32(40, 40, 40, 220);
            drawList->AddRectFilled(tl, br, bgCol, 3.0f);
            drawList->AddRect(tl, br, IM_COL32(80, 80, 80, 255), 3.0f);

            const auto& slot = slots_[idx];
            if (slot.itemId > 0) {
                // Show item ID as coloured text (replace with texture lookup when available).
                char label[16];
                snprintf(label, sizeof(label), "#%u", slot.itemId);
                ImVec2 textPos(x + 4.0f, y + 4.0f);
                drawList->AddText(textPos, IM_COL32(220, 220, 100, 255), label);

                // Stack count in bottom-right corner (if > 1).
                if (slot.quantity > 1) {
                    char qtyLabel[12];
                    snprintf(qtyLabel, sizeof(qtyLabel), "%u", slot.quantity);
                    ImVec2 qtyPos(x + kSlotSize - 20.0f, y + kSlotSize - 14.0f);
                    drawList->AddText(qtyPos, IM_COL32(255, 255, 255, 255), qtyLabel);
                }
            }

            // Invisible button for interaction.
            ImGui::SetCursorScreenPos(tl);
            std::string btnId = "##slot" + std::to_string(idx);
            if (ImGui::InvisibleButton(btnId.c_str(), ImVec2(kSlotSize, kSlotSize))) {
                if (slot.itemId > 0) {
                    std::cout << "[Inventory] Clicked slot " << idx
                              << " item=" << slot.itemId << "\n";
                    // TODO: publish use/equip ActionRequestEvent when item registry exists.
                }
            }

            // Drag source.
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                dragSrc_ = idx;
                ImGui::SetDragDropPayload("INV_SLOT", &idx, sizeof(int));
                char tooltip[32];
                snprintf(tooltip, sizeof(tooltip), "Item #%u", slot.itemId);
                ImGui::Text("%s", tooltip);
                ImGui::EndDragDropSource();
            }

            // Drop target.
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("INV_SLOT")) {
                    int srcIdx = *static_cast<const int*>(payload->Data);
                    if (srcIdx != idx) {
                        std::cout << "[Inventory] Move slot " << srcIdx
                                  << " → " << idx << "\n";
                        // Optimistic local swap: update the display immediately for
                        // responsiveness.  The server's authoritative InventorySyncPacket
                        // will be sent back and will correct any discrepancy if the move
                        // was rejected or if the packet was reordered.
                        std::swap(slots_[srcIdx], slots_[idx]);
                        // Publish InventoryMoveEvent so NetworkSystem can send
                        // InventoryMovePacket to the server for authoritative validation.
                        InventoryMoveEvent moveEvt{};
                        moveEvt.srcSlot = static_cast<uint8_t>(srcIdx);
                        moveEvt.dstSlot = static_cast<uint8_t>(idx);
                        EventBus::instance().publish(moveEvt);
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }
    }

    // Advance cursor past the grid.
    ImGui::SetCursorScreenPos(
        ImVec2(origin.x,
               origin.y + kRows * (kSlotSize + padding) + padding));

    // Register the window bounds as a UI region so InputDispatcher
    // skips world interactions when the cursor is over the inventory.
    ImVec2 wPos  = ImGui::GetWindowPos();
    ImVec2 wSize = ImGui::GetWindowSize();
    UiMaster::registerUiRegion(wPos.x, wPos.y, wSize.x, wSize.y);

    ImGui::End();
}
