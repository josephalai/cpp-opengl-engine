//
// Created by Joseph Alai on 7/10/21.
//

#ifndef ENGINE_PLAYER_H
#define ENGINE_PLAYER_H
#include "Entity.h"
#include "../RenderEngine/DisplayManager.h"
#include "../Terrain/Terrain.h"
#include "Components/InputComponent.h"

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

    void move(Terrain *terrain);

    /// Wire the player to a live PhysicsSystem so Bullet handles gravity and
    /// collision instead of the manual terrain-height fallback in move().
    void setPhysicsSystem(PhysicsSystem* ps);

    /// Subscribe to PlayerMoveCommandEvent on the global EventBus.
    /// After this call, checkInputs() skips direct InputMaster polling and
    /// relies on the event handler to keep currentSpeed / currentTurnSpeed
    /// up to date.  Call once during engine initialisation (after InputDispatcher
    /// has been registered as an ISystem).
    void subscribeToEvents();

private:
    InputComponent* inputComponent_ = nullptr; ///< non-owning ptr; owned by components_
};
#endif //ENGINE_PLAYER_H

