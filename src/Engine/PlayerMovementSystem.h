#ifndef ENGINE_PLAYERMOVEMENTSYSTEM_H
#define ENGINE_PLAYERMOVEMENTSYSTEM_H

#include "ISystem.h"
#include "../Events/Event.h"
#include <entt/entt.hpp>

class PlayerMovementSystem : public ISystem {
public:
    /// Distance from origin that triggers an origin shift (Phase 4 Step 4.2).
    static constexpr float kOriginShiftThreshold = 4000.0f;

    explicit PlayerMovementSystem(entt::registry& registry);

    void init()     override;
    void update(float deltaTime) override;
    void shutdown() override {}

private:
    entt::registry& registry_;

    // Latest movement command received from the EventBus (set in init()).
    PlayerMoveCommandEvent pendingCmd_{};
};

#endif // ENGINE_PLAYERMOVEMENTSYSTEM_H
