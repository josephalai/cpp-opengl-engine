//
// PhysicsSystem.h — Bullet Physics integration as an ISystem subsystem.
// Manages the Bullet dynamics world, rigid body registration, and
// physics-to-ECS TransformComponent synchronisation.
//
// Build modes
// -----------
// Client build  (HEADLESS_SERVER not defined) — full pipeline including debug
//   drawing (PhysicsDebugDrawer), the OOP Entity* convenience API, and the
//   Bullet kinematic character controller for the local Player.
//
// Server build  (HEADLESS_SERVER defined) — pure simulation:  no OpenGL, no
//   GLFW, no Player*.  All bodies are registered and synced via entt::entity
//   handles and TransformComponent; the debug drawer is completely absent.
//

#ifndef ENGINE_PHYSICSSYSTEM_H
#define ENGINE_PHYSICSSYSTEM_H

#include "../Engine/ISystem.h"
#include "PhysicsComponents.h"
#include <entt/entt.hpp>
#include "../ECS/Components/TransformComponent.h"

// Entity.h (OOP wrapper) is only needed by the client-side Entity* convenience API.
#ifndef HEADLESS_SERVER
#include "../Entities/Entity.h"
#endif

#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <BulletDynamics/Character/btKinematicCharacterController.h>

#include <unordered_map>
#include <vector>

// Forward declarations
#ifndef HEADLESS_SERVER
class PhysicsDebugDrawer;
class Player;
#endif
class Terrain;

/// One record kept per registered entity.
/// Maps a Bullet rigid body to an entt::entity handle so transform sync
/// can go directly through the ECS registry — no OOP Entity* pointer needed.
struct PhysicsEntry {
    entt::entity  entityHandle = entt::null;  ///< ECS handle for TransformComponent lookup.
    btRigidBody*  body         = nullptr;
    BodyType      bodyType     = BodyType::Dynamic;
};

/// Holds all Bullet objects that make up a character controller for one ECS entity.
/// Used by the ECS-native API (server: all players/NPCs; client: remote players).
/// Kept outside the HEADLESS_SERVER guard so both builds can share the type.
struct CharacterControllerData {
    btPairCachingGhostObject*       ghostObject       = nullptr;
    btKinematicCharacterController* controller        = nullptr;
    btCapsuleShape*                 capsuleShape      = nullptr;
    float                           capsuleHalfHeight = 0.0f; ///< capsuleHeight/2 + radius
};

class PhysicsSystem : public ISystem {
public:
    PhysicsSystem();
    ~PhysicsSystem() override;

    // ISystem
    void init()                  override;
    void update(float deltaTime) override;
    void shutdown()              override;

    /// Bind the ECS registry the system reads/writes TransformComponent from.
    /// Must be called before init() when running as the headless server, and
    /// before any body-registration calls on the client.
    void setRegistry(entt::registry& reg) { registry_ = &reg; }

    /// World accessor so other systems can query/rayCast.
    btDiscreteDynamicsWorld* getWorld() { return dynamicsWorld_; }

    // -------------------------------------------------------------------------
    // ECS-native registration API — used by both server and client.
    // Initial position/rotation are read from the entity's TransformComponent
    // (registry_ must be set before calling these).
    // -------------------------------------------------------------------------

    /// Register an entt::entity with a static (mass=0) rigid body.
    void addStaticBody(entt::entity entity, ColliderShape shape,
                       glm::vec3 halfExtents = glm::vec3(0.5f),
                       float friction = 0.5f, float restitution = 0.3f);

    /// Register an entt::entity with a dynamic rigid body.
    void addDynamicBody(entt::entity entity, const PhysicsBodyDef& def);

    /// Register an entt::entity with a kinematic rigid body.
    void addKinematicBody(entt::entity entity, const PhysicsBodyDef& def);

    /// Unregister an entity's rigid body (by entt handle).
    void removeRigidBody(entt::entity entity);

    /// Add an infinite static ground plane at the given y height.
    void addGroundPlane(float yHeight = 0.0f);

    /// Add a static heightfield collider for a Terrain tile.
    /// (Client-only in practice; server uses HeadlessTerrain for height clamping.)
    void addTerrainCollider(Terrain* terrain);

    /// Add a static heightfield collider using raw height data.
    /// Available on both server and client — no OpenGL types required.
    /// @param heights     2-D height grid (heights[x][z], square: NxN)
    /// @param terrainSize World-space width/depth of the tile (e.g. 800.0f)
    /// @param originX     World X of the tile's minimum corner
    /// @param originZ     World Z of the tile's minimum corner
    void addHeadlessTerrainCollider(const std::vector<std::vector<float>>& heights,
                                    float terrainSize, float originX, float originZ);

    // -------------------------------------------------------------------------
    // ECS-native character controller API — server + client (no GL/GLFW needed)
    // -------------------------------------------------------------------------

    /// Register a Bullet kinematic character controller for the given entity.
    /// Reads initial position from the entity's TransformComponent.
    /// Bullet's world gravity drives vertical movement; horizontal intent is
    /// supplied via setEntityWalkDirection each tick.
    void addCharacterController(entt::entity entity,
                                float radius = 0.5f,
                                float height = 1.8f);

    /// Set the intended horizontal displacement for the next physics step.
    /// Only the XZ components of walkDisplacement are applied; Bullet's
    /// built-in gravity controller handles the Y axis autonomously.
    void setEntityWalkDirection(entt::entity entity, glm::vec3 walkDisplacement);

    /// Trigger a jump on the entity's character controller.
    /// No-ops if the controller is already airborne (canJump() returns false).
    void jumpCharacterController(entt::entity entity);

    /// Remove and destroy the character controller for the given entity.
    /// Must be called before registry.destroy(entity).
    void removeCharacterController(entt::entity entity);

    /// Returns true if a character controller has been registered for entity.
    bool hasCharacterController(entt::entity entity) const;

    /// Teleport a character controller to a new position.
    /// @param feetPos  World-space feet position (ground level); the ghost
    ///                 capsule centre is placed at feetPos.y + capsuleHalfHeight.
    void warpCharacterController(entt::entity entity, const glm::vec3& feetPos);

#ifndef HEADLESS_SERVER
    // -------------------------------------------------------------------------
    // Legacy OOP convenience wrappers — client only.
    // These extract the entt::entity handle from the Entity* and delegate to
    // the ECS-native API above.  registry_ must still be set.
    // -------------------------------------------------------------------------

    /// Register an Entity* with a static rigid body.
    void addStaticBody(Entity* entity, ColliderShape shape,
                       glm::vec3 halfExtents = glm::vec3(0.5f),
                       float friction = 0.5f, float restitution = 0.3f);

    /// Register an Entity* with a dynamic rigid body.
    void addDynamicBody(Entity* entity, const PhysicsBodyDef& def);

    /// Register an Entity* with a kinematic rigid body.
    void addKinematicBody(Entity* entity, const PhysicsBodyDef& def);

    /// Unregister an Entity*'s rigid body.
    void removeRigidBody(Entity* entity);

    // -------------------------------------------------------------------------
    // Debug drawing — client only (requires OpenGL context).
    // -------------------------------------------------------------------------

    /// Enable/disable the debug drawer (toggled via F3 in update()).
    void setDebugDrawEnabled(bool enabled) { debugDrawEnabled_ = enabled; }
    bool isDebugDrawEnabled() const        { return debugDrawEnabled_; }

    /// Upload and render accumulated debug lines.
    /// Call this after all render systems have finished their pass.
    void renderDebug(const glm::mat4& view, const glm::mat4& projection);

    // -------------------------------------------------------------------------
    // Kinematic character controller — client only (local Player).
    // -------------------------------------------------------------------------

    /// Set up a Bullet kinematic character controller for the player.
    void setCharacterController(Player* player,
                                float capsuleRadius = 0.5f,
                                float capsuleHeight = 1.8f);

    /// Drive the character controller's walk direction each frame.
    /// vx/vz are per-frame displacements (velocity × dt) in world space.
    /// wantsJump triggers a single jump when the character is grounded.
    void setPlayerWalkDirection(float vx, float vz, bool wantsJump = false);

    void warpPlayer(const glm::vec3& feetPos);

    /// Sync the ghost object's world transform back to the Player entity.
    void syncCharacterToPlayer();

    /// Distance from capsule centre to capsule bottom (height/2 + radius).
    float getCapsuleHalfHeight() const { return capsuleHalfHeight_; }
#endif // !HEADLESS_SERVER

private:
    btDefaultCollisionConfiguration*     collisionConfig_    = nullptr;
    btCollisionDispatcher*               dispatcher_         = nullptr;
    btDbvtBroadphase*                    broadphase_         = nullptr;
    btSequentialImpulseConstraintSolver* solver_             = nullptr;
    btDiscreteDynamicsWorld*             dynamicsWorld_      = nullptr;

    /// Non-owning pointer to the engine/server registry for TransformComponent sync.
    entt::registry*                      registry_           = nullptr;

    std::vector<PhysicsEntry>                     entries_;
    std::vector<btCollisionShape*>                shapes_;         ///< owned base (child) shapes
    std::vector<btCompoundShape*>                 compoundShapes_; ///< wrapper shapes (deleted before children)
    std::vector<btRigidBody*>                     groundBodies_;   ///< ground planes + terrain tiles
    std::vector<btCollisionShape*>                groundShapes_;
    std::vector<std::vector<float>>               terrainHeightBuffers_; ///< keeps height data alive for terrain shapes

    /// ECS-native character controllers (server: all players/NPCs; client: remote players).
    /// Keyed by the raw uint32_t value of entt::entity to avoid needing a hash specialisation.
    std::unordered_map<uint32_t, CharacterControllerData> characterControllers_;

#ifndef HEADLESS_SERVER
    PhysicsDebugDrawer*                  debugDrawer_        = nullptr;
    bool                                 debugDrawEnabled_   = false;

    // Character controller (local Player only — not replicated to server)
    Player*                            playerPtr_           = nullptr;
    btPairCachingGhostObject*          ghostObject_         = nullptr;
    btKinematicCharacterController*    characterController_ = nullptr;
    btCapsuleShape*                    capsuleShape_        = nullptr;
    float                              capsuleHalfHeight_   = 0.0f;  ///< height/2 + radius
#endif // !HEADLESS_SERVER

    /// Build a btCollisionShape from a PhysicsBodyDef.
    btCollisionShape* createShape(const PhysicsBodyDef& def);

    /// Internal implementation: registers a Bullet body for the given entt::entity
    /// using the supplied initial world position and rotation.
    /// Called by both the ECS-native API and the legacy Entity* wrappers.
    void addDynamicBodyInternal(entt::entity entity, const PhysicsBodyDef& def,
                                glm::vec3 position, glm::vec3 rotation);
};

#endif // ENGINE_PHYSICSSYSTEM_H
