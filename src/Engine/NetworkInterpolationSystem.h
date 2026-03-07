#ifndef ENGINE_NETWORKINTERPOLATIONSYSTEM_H
#define ENGINE_NETWORKINTERPOLATIONSYSTEM_H

#include "ISystem.h"
#include <entt/entt.hpp>

class NetworkInterpolationSystem : public ISystem {
public:
    explicit NetworkInterpolationSystem(entt::registry& registry);

    void init()   override {}
    void update(float deltaTime) override;
    void shutdown() override {}

private:
    entt::registry& registry_;
};

#endif // ENGINE_NETWORKINTERPOLATIONSYSTEM_H
