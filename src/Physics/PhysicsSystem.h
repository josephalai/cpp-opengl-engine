//
// PhysicsSystem.h — Bullet Physics integration as an ISystem subsystem.
// Manages the Bullet dynamics world, rigid body registration, and
// physics-to-renderer transform synchronisation.
//

#ifndef ENGINE_PHYSICSSYSTEM_H
#define ENGINE_PHYSICSSYSTEM_H

#include "../Engine/ISystem.h"
#include "../Entities/Entity.h"
#include "../Entities/AssimpEntity.h"
#include "PhysicsComponents.h"

#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <BulletDynamics/Character/btKinematicCharacterController.h>

#include <unordered_map>
#include <vector>

// Forward declaration
class PhysicsDebugDrawer;
class Player;

/// One record kept per registered entity.
struct PhysicsEntry {
    Entity*       entity   = nullptr;  ///< null for non-Entity registrations
    AssimpEntity* aEntity  = nullptr;  ///< null for non-AssimpEntity registrations
    btRigidBody*  body     = nullptr;
    BodyType      bodyType = BodyType::Dynamic;
};

class PhysicsSystem : public ISystem {
public:
    PhysicsSystem();
    ~PhysicsSystem() override;

    // ISystem
    void init()                  override;
    void update(float deltaTime) override;
    void shutdown()              override;

    /// World accessor so other systems can query/rayCast.
    btDiscreteDynamicsWorld* getWorld() { return dynamicsWorld_; }

    // --- Entity registration helpers ---

    /// Register an Entity with a static (mass=0) rigid body.
    void addStaticBody(Entity* entity, ColliderShape shape,
                       glm::vec3 halfExtents = glm::vec3(0.5f),
                       float friction = 0.5f, float restitution = 0.3f);

    /// Register an Entity with a dynamic rigid body.
    void addDynamicBody(Entity* entity, const PhysicsBodyDef& def);

    /// Register an Entity with a kinematic rigid body.
    void addKinematicBody(Entity* entity, const PhysicsBodyDef& def);

    /// Unregister an entity's rigid body.
    void removeRigidBody(Entity* entity);

    /// Add an infinite static ground plane at the given y height.
    void addGroundPlane(float yHeight = 0.0f);

    /// Enable/disable the debug drawer.
    void setDebugDrawEnabled(bool enabled) { debugDrawEnabled_ = enabled; }
    bool isDebugDrawEnabled() const        { return debugDrawEnabled_; }

    /// Render accumulated debug lines.  Call after render systems have run.
    void renderDebug(const glm::mat4& view, const glm::mat4& projection);

    /// Set up a kinematic character controller for the player.
    void setCharacterController(Player* player,
                                float capsuleRadius = 0.5f,
                                float capsuleHeight = 1.8f);

    /// Drive the character controller's walk direction (call from Player::move()).
    /// vx/vz are world-space velocity in units/second; wantsJump triggers a jump
    /// if the character is on the ground.
    void setPlayerWalkDirection(float vx, float vz, bool wantsJump = false);

    /// Sync character controller ghost transform back to player after step.
    void syncCharacterToPlayer();

private:
    btDefaultCollisionConfiguration*     collisionConfig_    = nullptr;
    btCollisionDispatcher*               dispatcher_         = nullptr;
    btDbvtBroadphase*                    broadphase_         = nullptr;
    btSequentialImpulseConstraintSolver* solver_             = nullptr;
    btDiscreteDynamicsWorld*             dynamicsWorld_      = nullptr;

    PhysicsDebugDrawer*                  debugDrawer_        = nullptr;
    bool                                 debugDrawEnabled_   = false;

    std::vector<PhysicsEntry>                     entries_;
    std::vector<btCollisionShape*>                shapes_;        ///< owned shapes
    std::vector<btRigidBody*>                     groundBodies_;  ///< ground planes
    std::vector<btCollisionShape*>                groundShapes_;

    // Character controller
    Player*                            playerPtr_           = nullptr;
    btPairCachingGhostObject*          ghostObject_         = nullptr;
    btKinematicCharacterController*    characterController_ = nullptr;
    btCapsuleShape*                    capsuleShape_        = nullptr;

    /// Build a btCollisionShape from a PhysicsBodyDef.
    btCollisionShape* createShape(const PhysicsBodyDef& def);
};

#endif // ENGINE_PHYSICSSYSTEM_H
