//
// Created by Joseph Alai on 7/10/21.
//

#ifndef ENGINE_PLAYER_H
#define ENGINE_PLAYER_H
#include "Entity.h"
#include "../RenderEngine/DisplayManager.h"
#include "../Terrain/Terrain.h"

class PhysicsSystem;  ///< forward declaration — avoids circular include

class Player : public Entity {
public:
    /**
     * @brief Player extends Entity: so it stores TexturedModel, as well as its' vectors
     *        to be able to manipulate the model. It also checks for input, and allows
     *        the user to move around, zoom in and out, etc.
     * @param model
     * @param position
     * @param rotation
     * @param scale
     */
    Player(entt::registry& registry, TexturedModel *model, BoundingBox *box,
           glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f),
           glm::vec3 rotation = glm::vec3(0), float scale = 1.0f);

    /// No-op stub retained for call-site compatibility (PlayerCamera::move).
    void move(Terrain *terrain);

    /// No-op stub retained for call-site compatibility (Engine::loadScene).
    void setPhysicsSystem(PhysicsSystem* ps);
};
#endif //ENGINE_PLAYER_H

