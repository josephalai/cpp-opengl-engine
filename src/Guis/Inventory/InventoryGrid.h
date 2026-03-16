// src/Guis/Inventory/InventoryGrid.h
//
// Phase 4 — Client-side inventory grid.
//
// InventoryGrid is an ImGui-based 4×7 slot grid that visualises the player's
// server-authoritative inventory.  The client receives InventorySyncPackets,
// which are stored here; left-clicking a filled slot publishes an
// EntityClickedEvent-derived action (equip/use); drag-dropping sends an
// InventoryMovePacket via an InventoryMoveEvent.
//
// All actual inventory state lives on the server; this grid is a read-only
// mirror updated by sync packets.

#ifndef ENGINE_INVENTORYGRID_H
#define ENGINE_INVENTORYGRID_H

#include "../../Network/NetworkPackets.h"
#include <array>
#include <string>
#include <cstdint>

/// Minimal client-side item descriptor (just ID + quantity from the sync packet).
struct ClientItemSlot {
    uint32_t itemId   = 0;
    uint32_t quantity = 0;
};

class InventoryGrid {
public:
    /// Singleton accessor.
    static InventoryGrid& instance();

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    /// Subscribe to InventorySyncEvent and InventoryMoveEvent via the EventBus.
    /// Must be called once before the first render frame.
    void init();

    // ------------------------------------------------------------------
    // Data update (called by NetworkSystem on InventorySyncPacket arrival)
    // ------------------------------------------------------------------
    void applySync(const Network::InventorySyncPacket& pkt);

    // ------------------------------------------------------------------
    // Per-frame rendering
    // ------------------------------------------------------------------
    void render();

    void show()   { visible_ = true;  }
    void hide()   { visible_ = false; }
    void toggle() { visible_ = !visible_; }
    bool isVisible()  const { return visible_; }
    bool isDragging() const { return dragSrc_ >= 0; }

private:
    InventoryGrid() = default;

    static constexpr int kSlots  = Network::kInventorySlots; ///< 28 slots
    static constexpr int kCols   = 4;
    static constexpr int kRows   = kSlots / kCols;           ///< 7 rows
    static constexpr float kSlotSize = 36.0f;

    bool visible_ = false;

    std::array<ClientItemSlot, kSlots> slots_{};

    /// Index of the slot currently being dragged (-1 = none).
    int dragSrc_ = -1;
};

#endif // ENGINE_INVENTORYGRID_H
