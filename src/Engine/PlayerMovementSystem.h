#ifndef ENGINE_PLAYERMOVEMENTSYSTEM_H
#define ENGINE_PLAYERMOVEMENTSYSTEM_H

#include "ISystem.h"
#include <entt/entt.hpp>

class PlayerMovementSystem : public ISystem {
public:
    explicit PlayerMovementSystem(entt::registry& registry);

    void init()   override {}
    void update(float deltaTime) override;
    void shutdown() override {}

private:
    entt::registry& registry_;
};

#endif // ENGINE_PLAYERMOVEMENTSYSTEM_H
