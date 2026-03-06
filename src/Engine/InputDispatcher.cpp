// src/Engine/InputDispatcher.cpp

#include "InputDispatcher.h"
#include "../Events/Event.h"
#include "../Events/EventBus.h"
#include "../Input/InputMaster.h"
#include "../Toolbox/TerrainPicker.h"
#include <iostream>

InputDispatcher::InputDispatcher(TerrainPicker* picker)
    : picker_(picker)
{}

void InputDispatcher::update(float /*deltaTime*/) {
    // --- Movement command ---
    // Translate WASD / Space / Tab / Backslash into a normalised command
    // and broadcast it.  Subscribers (e.g. Player) consume this instead of
    // polling InputMaster::isKeyDown() inside their own update loops.
    PlayerMoveCommandEvent moveCmd{};
    moveCmd.forward     = InputMaster::isKeyDown(W)         ?  1.0f
                        : InputMaster::isKeyDown(S)         ? -1.0f : 0.0f;
    moveCmd.turn        = InputMaster::isKeyDown(A)         ?  1.0f
                        : InputMaster::isKeyDown(D)         ? -1.0f : 0.0f;
    moveCmd.jump        = InputMaster::isKeyDown(Space);
    moveCmd.sprint      = InputMaster::isKeyDown(Tab);
    moveCmd.sprintReset = InputMaster::isKeyDown(Backslash);

    // [NetTrace] Log non-zero movement commands (throttled)
    if ((moveCmd.forward != 0.0f || moveCmd.turn != 0.0f) &&
        logThrottleCounter_++ % kLogThrottleInterval == 0) {
        std::cout << "[NetTrace][InputDispatcher] PlayerMoveCommandEvent"
                     " fwd=" << moveCmd.forward
                  << " turn=" << moveCmd.turn << "\n";
    }

    EventBus::instance().publish(moveCmd);

    // --- Terrain right-click (rising edge) → TargetLocationClickedEvent ---
    // Only publish once per press (rising-edge detection) to avoid flooding
    // subscribers with repeated events while the button is held.
    bool rightNow = InputMaster::isMouseDown(RightClick);
    if (rightNow && !prevRightClick_ && picker_) {
        glm::vec3 pt = picker_->getCurrentTerrainPoint();
        // getCurrentTerrainPoint() returns a zero-vector when no terrain
        // intersection was found; skip publishing in that case.
        if (pt != glm::vec3(0.0f)) {
            TargetLocationClickedEvent evt{};
            evt.worldPosition = pt;
            EventBus::instance().publish(evt);
        }
    }
    prevRightClick_ = rightNow;
}
