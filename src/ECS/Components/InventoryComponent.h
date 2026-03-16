// src/ECS/Components/InventoryComponent.h
//
// Phase 4 — Server-authoritative inventory.
// Attached to every player entity on the headless server.  The server is the
// single source of truth; the client receives read-only snapshots via
// InventorySyncPacket whenever the contents change.

#ifndef ECS_INVENTORYCOMPONENT_H
#define ECS_INVENTORYCOMPONENT_H

#include "../../Network/NetworkPackets.h"
#include <cstdint>
#include <array>

/// Lightweight item descriptor stored per-slot.
struct ItemSlot {
    uint32_t itemId   = 0; ///< 0 = empty.
    uint32_t quantity = 0; ///< Stack size; always ≥1 when itemId > 0.
};

/// Server-side inventory attached to each connected player entity.
/// Capacity is fixed at kInventorySlots (28) to match the classic OSRS grid.
struct InventoryComponent {
    static constexpr int kSlots = Network::kInventorySlots;

    std::array<ItemSlot, kSlots> slots{};

    // -----------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------

    /// Returns the first empty slot index, or -1 if the inventory is full.
    int firstEmpty() const {
        for (int i = 0; i < kSlots; ++i) {
            if (slots[i].itemId == 0) return i;
        }
        return -1;
    }

    /// Attempt to add one item (stacking if the same ID is already present).
    /// Returns true on success, false if the inventory is full.
    bool addItem(uint32_t itemId, uint32_t qty = 1) {
        // Try stacking first.
        for (auto& s : slots) {
            if (s.itemId == itemId) { s.quantity += qty; return true; }
        }
        int idx = firstEmpty();
        if (idx < 0) return false;
        slots[idx] = {itemId, qty};
        return true;
    }

    /// Remove `qty` units of `itemId`.  Returns true if successful.
    bool removeItem(uint32_t itemId, uint32_t qty = 1) {
        for (auto& s : slots) {
            if (s.itemId == itemId && s.quantity >= qty) {
                s.quantity -= qty;
                if (s.quantity == 0) s.itemId = 0;
                return true;
            }
        }
        return false;
    }

    /// Swap two slots (client-requested move).
    void swapSlots(int src, int dst) {
        if (src < 0 || src >= kSlots || dst < 0 || dst >= kSlots) return;
        std::swap(slots[src], slots[dst]);
    }

    /// Build a wire-ready InventorySyncPacket from the current state.
    Network::InventorySyncPacket toSyncPacket() const {
        Network::InventorySyncPacket pkt{};
        for (int i = 0; i < kSlots; ++i) {
            pkt.itemIds[i]    = slots[i].itemId;
            pkt.quantities[i] = slots[i].quantity;
        }
        return pkt;
    }
};

#endif // ECS_INVENTORYCOMPONENT_H
