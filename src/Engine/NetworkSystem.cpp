// src/Engine/NetworkSystem.cpp

#include "NetworkSystem.h"
#include "../Network/MockServer.h"

NetworkSystem::NetworkSystem(std::vector<Entity*> netEntities)
    : netEntities_(std::move(netEntities))
{}

void NetworkSystem::update(float deltaTime) {
    // Advance the mock server clock; fires a 10 Hz tick when due.
    MockServer::instance().update(deltaTime);

    // Drive interpolation on every network-controlled entity.
    for (Entity* e : netEntities_) {
        if (e) {
            e->updateComponents(deltaTime);
        }
    }
}

void NetworkSystem::addEntity(Entity* e) {
    if (e) {
        netEntities_.push_back(e);
    }
}
