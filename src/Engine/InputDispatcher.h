// src/Engine/InputDispatcher.h
// An ISystem that runs at the top of the engine loop to translate raw
// InputMaster state into typed EventBus events.
//
// This is the single bridge between hardware input and gameplay logic.
// Systems that previously polled InputMaster::isKeyDown() for movement should
// instead subscribe to PlayerMoveCommandEvent via EventBus::instance().
// Systems that need the terrain right-click point should subscribe to
// TargetLocationClickedEvent instead of querying TerrainPicker directly.
//
// Ordering constraint: InputDispatcher must be the first ISystem registered
// in Engine::buildSystems() so that event subscribers have up-to-date values
// before any other system's update() runs.

#ifndef ENGINE_INPUTDISPATCHER_H
#define ENGINE_INPUTDISPATCHER_H

#include "ISystem.h"
#include <glm/glm.hpp>

class TerrainPicker;

class InputDispatcher : public ISystem {
public:
    /// @param picker  Optional terrain picker used to resolve right-click
    ///               terrain intersections for TargetLocationClickedEvent.
    explicit InputDispatcher(TerrainPicker* picker = nullptr);

    void init()     override {}
    void update(float deltaTime) override;
    void shutdown() override {}

private:
    TerrainPicker* picker_;
    bool           prevRightClick_ = false; ///< edge-detect: fire event only once per press
};

#endif // ENGINE_INPUTDISPATCHER_H
