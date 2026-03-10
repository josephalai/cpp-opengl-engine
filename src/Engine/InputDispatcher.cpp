// src/Engine/InputDispatcher.cpp

#include "InputDispatcher.h"
#include "../Events/Event.h"
#include "../Events/EventBus.h"
#include "../Input/InputMaster.h"
#include "../Toolbox/TerrainPicker.h"

InputDispatcher::InputDispatcher(TerrainPicker* picker)
    : picker_(picker)
{}

void InputDispatcher::update(float /*deltaTime*/) {
    // --- Movement command ---
    // Translate WASD / Space / Tab / Backslash into a normalised command
    // and broadcast it.  Subscribers (e.g. Player) consume this instead of
    // polling InputMaster::isKeyDown() inside their own update loops.
    PlayerMoveCommandEvent moveCmd{};
    moveCmd.forward     = InputMaster::isActionDown("MoveForward")  ?  1.0f
                        : InputMaster::isActionDown("MoveBackward") ? -1.0f : 0.0f;
    moveCmd.turn        = InputMaster::isActionDown("MoveLeft")     ?  1.0f
                        : InputMaster::isActionDown("MoveRight")    ? -1.0f : 0.0f;
    moveCmd.jump        = InputMaster::isActionDown("Jump");
    moveCmd.sprint      = InputMaster::isActionDown("Sprint");
    moveCmd.sprintReset = InputMaster::isActionDown("SprintReset");

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
