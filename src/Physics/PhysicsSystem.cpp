//
// PhysicsSystem.cpp — implementation of the Bullet Physics subsystem.
//

#include "PhysicsSystem.h"
#include "../ECS/Components/TransformComponent.h"
#include "../Config/ConfigManager.h"

#ifndef HEADLESS_SERVER
#include "PhysicsDebugDrawer.h"
#include "../Entities/Player.h"
#include "../Terrain/Terrain.h"
#include "../Input/InputMaster.h"
#endif

#include <BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>

#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

// ---------------------------------------------------------------------------
// Helpers: convert between Bullet and GLM
// ---------------------------------------------------------------------------

static btVector3 glmToBt(const glm::vec3& v) {
    return btVector3(v.x, v.y, v.z);
}

static glm::vec3 btToGlm(const btVector3& v) {
    return glm::vec3(v.x(), v.y(), v.z());
}

/// Convert a btQuaternion to Euler angles in degrees (matching engine convention).
static glm::vec3 btQuatToEulerDeg(const btQuaternion& q) {
    glm::quat gq(q.w(), q.x(), q.y(), q.z());
    glm::vec3 euler = glm::degrees(glm::eulerAngles(gq));
    return euler;
}

// ---------------------------------------------------------------------------

PhysicsSystem::PhysicsSystem() = default;

PhysicsSystem::~PhysicsSystem() {
    shutdown();
}

void PhysicsSystem::init() {
    if (!registry_) {
        std::cerr << "[PhysicsSystem] WARNING: setRegistry() not called before init()."
                     " TransformComponent sync will be disabled.\n";
    }

    collisionConfig_ = new btDefaultCollisionConfiguration();
    dispatcher_      = new btCollisionDispatcher(collisionConfig_);
    broadphase_      = new btDbvtBroadphase();
    solver_          = new btSequentialImpulseConstraintSolver();
    dynamicsWorld_   = new btDiscreteDynamicsWorld(
        dispatcher_, broadphase_, solver_, collisionConfig_);

    // Read gravity from ConfigManager (data-driven; defaults to [0, -50, 0]).
    const auto& g = ConfigManager::get().physics.gravity;
    dynamicsWorld_->setGravity(btVector3(g.x, g.y, g.z));

    // Register the ghost-object callback needed by btKinematicCharacterController
    broadphase_->getOverlappingPairCache()->setInternalGhostPairCallback(
        new btGhostPairCallback());

#ifndef HEADLESS_SERVER
    // Create and register debug drawer (requires an active OpenGL context)
    debugDrawer_ = new PhysicsDebugDrawer();
    debugDrawer_->init();
    dynamicsWorld_->setDebugDrawer(debugDrawer_);
#endif
}

void PhysicsSystem::update(float deltaTime) {
    if (!dynamicsWorld_) return;

#ifndef HEADLESS_SERVER
    // F3 toggles physics debug rendering (client only — requires InputMaster/GLFW)
    static bool f3WasDown = false;
    bool f3IsDown = InputMaster::isKeyDown(F3);
    if (f3IsDown && !f3WasDown) {
        debugDrawEnabled_ = !debugDrawEnabled_;
        debugDrawer_->setDebugMode(debugDrawEnabled_
            ? btIDebugDraw::DBG_DrawWireframe | btIDebugDraw::DBG_DrawAabb
            : btIDebugDraw::DBG_NoDebug);
    }
    f3WasDown = f3IsDown;
#endif

    // Sync kinematic bodies FROM TransformComponent/registry TO physics world
    for (auto& e : entries_) {
        if (e.bodyType != BodyType::Kinematic) continue;
        glm::vec3 pos(0.0f), rot(0.0f);

        if (registry_ && e.entityHandle != entt::null) {
            if (auto* tc = registry_->try_get<TransformComponent>(e.entityHandle)) {
                pos = tc->position;
                rot = tc->rotation;
            }
        }

        glm::quat q = glm::quat(glm::radians(rot));
        btTransform t;
        t.setIdentity();
        t.setOrigin(glmToBt(pos));
        t.setRotation(btQuaternion(q.x, q.y, q.z, q.w));
        e.body->getMotionState()->setWorldTransform(t);
        e.body->setWorldTransform(t);
    }

    // Step simulation — single sub-step per call so the character controller
    // walk direction (which is a per-tick displacement, not per-sub-step) is
    // applied exactly once regardless of the tick interval duration.
    dynamicsWorld_->stepSimulation(deltaTime, 1, deltaTime);

#ifndef HEADLESS_SERVER
    // Sync character controller ghost transform → player (client only)
    if (characterController_ && playerPtr_) {
        syncCharacterToPlayer();
    }
#endif

    // Sync ECS character controllers: ghost transform → TransformComponent.
    // Done on both client and server — server uses this for all players/NPCs.
    for (auto& [key, data] : characterControllers_) {
        if (!data.ghostObject) continue;
        entt::entity entity = static_cast<entt::entity>(key);
        const btTransform& t = data.ghostObject->getWorldTransform();
        glm::vec3 pos = btToGlm(t.getOrigin());
        pos.y -= data.capsuleHalfHeight;  // ghost origin is at capsule centre; entity position is at feet
        if (registry_ && entity != entt::null) {
            if (auto* tc = registry_->try_get<TransformComponent>(entity)) {
                tc->position = pos;
            }
        }
    }

    // Sync dynamic body transforms FROM physics world TO TransformComponent/registry
    for (auto& e : entries_) {
        if (e.bodyType != BodyType::Dynamic) continue;
        btTransform t;
        e.body->getMotionState()->getWorldTransform(t);
        glm::vec3 pos = btToGlm(t.getOrigin());
        glm::vec3 rot = btQuatToEulerDeg(t.getRotation());

        if (registry_ && e.entityHandle != entt::null) {
            if (auto* tc = registry_->try_get<TransformComponent>(e.entityHandle)) {
                tc->position = pos;
                tc->rotation = rot;
            }
        }
    }

#ifndef HEADLESS_SERVER
    // Optionally draw debug wireframes (client only)
    if (debugDrawEnabled_ && debugDrawer_) {
        dynamicsWorld_->debugDrawWorld();
    }
#endif
}

void PhysicsSystem::shutdown() {
    // Remove all rigid bodies
    if (dynamicsWorld_) {
        for (auto& e : entries_) {
            if (e.body) {
                dynamicsWorld_->removeRigidBody(e.body);
                delete e.body->getMotionState();
                delete e.body;
            }
        }
        for (auto* b : groundBodies_) {
            dynamicsWorld_->removeRigidBody(b);
            delete b->getMotionState();
            delete b;
        }
        // Clean up ECS-native character controllers (server players/NPCs, client remote players)
        for (auto& [key, data] : characterControllers_) {
            if (data.controller)  dynamicsWorld_->removeAction(data.controller);
            if (data.ghostObject) dynamicsWorld_->removeCollisionObject(data.ghostObject);
            delete data.controller;   data.controller  = nullptr;
            delete data.ghostObject;  data.ghostObject  = nullptr;
            delete data.capsuleShape; data.capsuleShape = nullptr;
        }
        characterControllers_.clear();
#ifndef HEADLESS_SERVER
        if (characterController_) {
            dynamicsWorld_->removeAction(characterController_);
            dynamicsWorld_->removeCollisionObject(ghostObject_);
        }
#endif
    }
    entries_.clear();
    groundBodies_.clear();

    // compoundShapes_ must be deleted before shapes_ (their children).
    // btCompoundShape does not own its children; deleting children first
    // would leave the compound holding dangling pointers.
    for (auto* s : compoundShapes_)  delete s;
    for (auto* s : shapes_)          delete s;
    for (auto* s : groundShapes_)    delete s;
    shapes_.clear();
    compoundShapes_.clear();
    groundShapes_.clear();

#ifndef HEADLESS_SERVER
    delete characterController_; characterController_ = nullptr;
    delete ghostObject_;          ghostObject_         = nullptr;
    delete capsuleShape_;         capsuleShape_        = nullptr;

    delete debugDrawer_;  debugDrawer_ = nullptr;
#endif

    delete dynamicsWorld_; dynamicsWorld_ = nullptr;
    delete solver_;        solver_        = nullptr;
    delete broadphase_;    broadphase_    = nullptr;
    delete dispatcher_;    dispatcher_    = nullptr;
    delete collisionConfig_; collisionConfig_ = nullptr;

    terrainHeightBuffers_.clear();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

btCollisionShape* PhysicsSystem::createShape(const PhysicsBodyDef& def) {
    switch (def.shape) {
        case ColliderShape::Sphere:
            return new btSphereShape(def.radius);
        case ColliderShape::Capsule:
            return new btCapsuleShape(def.radius, def.height);
        case ColliderShape::Box:
        default:
            return new btBoxShape(btVector3(def.halfExtents.x,
                                            def.halfExtents.y,
                                            def.halfExtents.z));
    }
}

// ---------------------------------------------------------------------------
// Internal implementation — shared by ECS-native and legacy Entity* APIs
// ---------------------------------------------------------------------------

void PhysicsSystem::addDynamicBodyInternal(entt::entity entity,
                                            const PhysicsBodyDef& def,
                                            glm::vec3 pos,
                                            glm::vec3 rot) {
    if (!dynamicsWorld_) return;

    // Build the primitive base shape.
    btCollisionShape* baseShape = createShape(def);

    // Visual models pivot at their feet; Bullet primitives pivot at their centre.
    // Wrap in a btCompoundShape that shifts the primitive up so the compound
    // origin sits at the entity's foot position, eliminating the visual gap.
    float yOffset = 0.0f;
    switch (def.shape) {
        case ColliderShape::Sphere:  yOffset = def.radius;                       break;
        case ColliderShape::Capsule: yOffset = def.height * 0.5f + def.radius;  break;
        case ColliderShape::Box:
        default:                     yOffset = def.halfExtents.y;                break;
    }
    auto* compound = new btCompoundShape();
    btTransform childT;
    childT.setIdentity();
    childT.setOrigin(btVector3(0.0f, yOffset, 0.0f));
    compound->addChildShape(childT, baseShape);

    compoundShapes_.push_back(compound);
    shapes_.push_back(baseShape);

    glm::quat q = glm::quat(glm::radians(rot));

    btTransform startTransform;
    startTransform.setIdentity();
    startTransform.setOrigin(glmToBt(pos));
    startTransform.setRotation(btQuaternion(q.x, q.y, q.z, q.w));

    btScalar mass = (def.type == BodyType::Static) ? 0.0f : def.mass;
    btVector3 localInertia(0, 0, 0);
    if (mass > 0.0f) compound->calculateLocalInertia(mass, localInertia);

    auto* motionState = new btDefaultMotionState(startTransform);
    btRigidBody::btRigidBodyConstructionInfo ci(mass, motionState, compound, localInertia);
    ci.m_friction    = def.friction;
    ci.m_restitution = def.restitution;

    auto* body = new btRigidBody(ci);

    if (def.type == BodyType::Kinematic) {
        body->setCollisionFlags(
            body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
        body->setActivationState(DISABLE_DEACTIVATION);
    }

    dynamicsWorld_->addRigidBody(body);

    PhysicsEntry entry;
    entry.entityHandle = entity;
    entry.body     = body;
    entry.bodyType = def.type;
    entries_.push_back(entry);
}

// ---------------------------------------------------------------------------
// Public ECS-native API (server + client)
// ---------------------------------------------------------------------------

void PhysicsSystem::addStaticBody(entt::entity entity, ColliderShape shape,
                                   glm::vec3 halfExtents, float friction,
                                   float restitution) {
    if (!dynamicsWorld_) return;
    PhysicsBodyDef def;
    def.type        = BodyType::Static;
    def.shape       = shape;
    def.mass        = 0.0f;
    def.halfExtents = halfExtents;
    def.friction    = friction;
    def.restitution = restitution;
    // Call the internal method directly to avoid the registry read in addDynamicBody.
    glm::vec3 pos(0.0f), rot(0.0f);
    if (registry_ && entity != entt::null) {
        if (auto* tc = registry_->try_get<TransformComponent>(entity)) {
            pos = tc->position;
            rot = tc->rotation;
        }
    }
    addDynamicBodyInternal(entity, def, pos, rot);
}

void PhysicsSystem::addDynamicBody(entt::entity entity, const PhysicsBodyDef& def) {
    if (!dynamicsWorld_) return;
    glm::vec3 pos(0.0f), rot(0.0f);
    if (registry_ && entity != entt::null) {
        if (auto* tc = registry_->try_get<TransformComponent>(entity)) {
            pos = tc->position;
            rot = tc->rotation;
        }
    }
    addDynamicBodyInternal(entity, def, pos, rot);
}

void PhysicsSystem::addKinematicBody(entt::entity entity, const PhysicsBodyDef& def) {
    if (!dynamicsWorld_) return;
    PhysicsBodyDef kDef = def;
    kDef.type = BodyType::Kinematic;
    addDynamicBody(entity, kDef);
}

void PhysicsSystem::removeRigidBody(entt::entity entity) {
    if (!dynamicsWorld_) return;
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
        if (it->entityHandle == entity) {
            dynamicsWorld_->removeRigidBody(it->body);
            delete it->body->getMotionState();
            delete it->body;
            entries_.erase(it);
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Terrain collider (client + server via forward-declared Terrain)
// ---------------------------------------------------------------------------

void PhysicsSystem::addGroundPlane(float yHeight) {
    if (!dynamicsWorld_) return;

    auto* shape = new btStaticPlaneShape(btVector3(0, 1, 0), yHeight);
    groundShapes_.push_back(shape);

    btTransform t;
    t.setIdentity();
    t.setOrigin(btVector3(0, 0, 0));

    auto* motionState = new btDefaultMotionState(t);
    btRigidBody::btRigidBodyConstructionInfo ci(0.0f, motionState, shape);
    ci.m_friction    = 0.5f;
    ci.m_restitution = 0.3f;

    auto* body = new btRigidBody(ci);
    dynamicsWorld_->addRigidBody(body);
    groundBodies_.push_back(body);
}

void PhysicsSystem::addTerrainCollider(Terrain* terrain) {
#ifndef HEADLESS_SERVER
    if (!dynamicsWorld_ || !terrain) return;

    const auto& h2d = terrain->heights;
    int vertexCount = static_cast<int>(h2d.size());
    if (vertexCount < 2) return;

    terrainHeightBuffers_.emplace_back(vertexCount * vertexCount);
    auto& buf = terrainHeightBuffers_.back();

    float minH =  std::numeric_limits<float>::max();
    float maxH =  std::numeric_limits<float>::lowest();
    for (int x = 0; x < vertexCount; ++x) {
        for (int z = 0; z < vertexCount; ++z) {
            float hv = h2d[x][z];
            buf[z * vertexCount + x] = hv;
            if (hv < minH) minH = hv;
            if (hv > maxH) maxH = hv;
        }
    }

    auto* shape = new btHeightfieldTerrainShape(
        vertexCount, vertexCount,
        buf.data(),
        /*heightScale=*/1.0f,
        minH, maxH,
        /*upAxis=*/1,
        PHY_FLOAT,
        /*flipQuadEdges=*/false);

    float terrainSize = terrain->getSize();
    float stickScale  = terrainSize / static_cast<float>(vertexCount - 1);
    shape->setLocalScaling(btVector3(stickScale, 1.0f, stickScale));

    groundShapes_.push_back(shape);

    btTransform t;
    t.setIdentity();
    t.setOrigin(btVector3(
        terrain->getX() + terrainSize * 0.5f,
        (minH + maxH) * 0.5f,
        terrain->getZ() + terrainSize * 0.5f));

    auto* motionState = new btDefaultMotionState(t);
    btRigidBody::btRigidBodyConstructionInfo ci(0.0f, motionState, shape);
    ci.m_friction    = 0.8f;
    ci.m_restitution = 0.1f;

    auto* body = new btRigidBody(ci);
    dynamicsWorld_->addRigidBody(body);
    groundBodies_.push_back(body);
#endif // !HEADLESS_SERVER
}

void PhysicsSystem::addHeadlessTerrainCollider(
        const std::vector<std::vector<float>>& heights,
        float terrainSize, float originX, float originZ) {
    if (!dynamicsWorld_) return;

    int vertexCount = static_cast<int>(heights.size());
    if (vertexCount < 2) return;

    terrainHeightBuffers_.emplace_back(vertexCount * vertexCount);
    int bufIdx = static_cast<int>(terrainHeightBuffers_.size()) - 1;
    auto& buf = terrainHeightBuffers_.back();
    // NOTE: btHeightfieldTerrainShape holds a raw pointer into buf.data() and
    // does NOT copy the data.  The buffer must remain alive for the lifetime of
    // the shape, hence it is stored in terrainHeightBuffers_.

    float minH =  std::numeric_limits<float>::max();
    float maxH =  std::numeric_limits<float>::lowest();
    for (int x = 0; x < vertexCount; ++x) {
        for (int z = 0; z < vertexCount; ++z) {
            float hv = heights[x][z];
            buf[z * vertexCount + x] = hv;
            if (hv < minH) minH = hv;
            if (hv > maxH) maxH = hv;
        }
    }

    auto* shape = new btHeightfieldTerrainShape(
        vertexCount, vertexCount,
        buf.data(),
        /*heightScale=*/1.0f,
        minH, maxH,
        /*upAxis=*/1,
        PHY_FLOAT,
        /*flipQuadEdges=*/false);

    float stickScale = terrainSize / static_cast<float>(vertexCount - 1);
    shape->setLocalScaling(btVector3(stickScale, 1.0f, stickScale));

    groundShapes_.push_back(shape);

    btTransform t;
    t.setIdentity();
    t.setOrigin(btVector3(
        originX + terrainSize * 0.5f,
        (minH + maxH) * 0.5f,
        originZ + terrainSize * 0.5f));

    auto* motionState = new btDefaultMotionState(t);
    btRigidBody::btRigidBodyConstructionInfo ci(0.0f, motionState, shape);
    ci.m_friction    = 0.8f;
    ci.m_restitution = 0.1f;

    auto* body = new btRigidBody(ci);
    dynamicsWorld_->addRigidBody(body);
    groundBodies_.push_back(body);

    // Track for dynamic removal by grid coordinates.
    int gx = static_cast<int>(std::floor(originX / terrainSize));
    int gz = static_cast<int>(std::floor(originZ / terrainSize));
    TerrainColliderRecord rec;
    rec.body        = body;
    rec.shape       = shape;
    rec.motion      = motionState;
    rec.bufferIndex = bufIdx;
    terrainColliders_[terrainGridKey(gx, gz)] = rec;
}

void PhysicsSystem::removeHeadlessTerrainCollider(int gridX, int gridZ) {
    if (!dynamicsWorld_) return;

    int64_t key = terrainGridKey(gridX, gridZ);
    auto it = terrainColliders_.find(key);
    if (it == terrainColliders_.end()) return;

    auto& rec = it->second;

    // Remove from Bullet world.
    dynamicsWorld_->removeRigidBody(rec.body);

    // Remove from groundBodies_ vector.
    auto bit = std::find(groundBodies_.begin(), groundBodies_.end(), rec.body);
    if (bit != groundBodies_.end()) groundBodies_.erase(bit);

    // Remove from groundShapes_ vector.
    auto sit = std::find(groundShapes_.begin(), groundShapes_.end(), rec.shape);
    if (sit != groundShapes_.end()) groundShapes_.erase(sit);

    // Free Bullet objects.
    delete rec.body;
    delete rec.motion;
    delete rec.shape;

    // Note: we intentionally do NOT erase the height buffer from
    // terrainHeightBuffers_ because it would invalidate indices of other
    // records.  The buffer memory is reclaimed when PhysicsSystem shuts down.

    terrainColliders_.erase(it);
}

// ---------------------------------------------------------------------------
// ECS-native character controller API — server + client
// ---------------------------------------------------------------------------

void PhysicsSystem::addCharacterController(entt::entity entity, float radius, float height) {
    if (!dynamicsWorld_) return;
    uint32_t key = static_cast<uint32_t>(entity);
    if (characterControllers_.count(key)) return;  // already registered

    glm::vec3 pos(0.0f);
    if (registry_ && entity != entt::null) {
        if (auto* tc = registry_->try_get<TransformComponent>(entity))
            pos = tc->position;
    }

    CharacterControllerData data;
    data.capsuleHalfHeight = height * 0.5f + radius;
    data.capsuleShape = new btCapsuleShape(radius, height);

    data.ghostObject = new btPairCachingGhostObject();
    btTransform t;
    t.setIdentity();
    t.setOrigin(btVector3(pos.x, pos.y + data.capsuleHalfHeight, pos.z));
    data.ghostObject->setWorldTransform(t);
    data.ghostObject->setCollisionShape(data.capsuleShape);
    data.ghostObject->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT
                                        | btCollisionObject::CF_KINEMATIC_OBJECT);
    // Prevent Bullet's broadphase from putting the ghost object to sleep and
    // silently culling its collision sweeps against static geometry.
    data.ghostObject->setActivationState(DISABLE_DEACTIVATION);

    dynamicsWorld_->addCollisionObject(
        data.ghostObject,
        btBroadphaseProxy::CharacterFilter,
        btBroadphaseProxy::StaticFilter | btBroadphaseProxy::DefaultFilter);

    data.controller = new btKinematicCharacterController(
        data.ghostObject, data.capsuleShape, 0.35f);
    // Let Bullet's world gravity drive vertical movement for this controller.
    // SharedMovement only supplies horizontal intent; this makes the character
    // land correctly on top of static geometry (trees, stalls) rather than
    // only knowing about the flat terrain plane.
    data.controller->setGravity(dynamicsWorld_->getGravity());
    // Configure jump speed from ConfigManager (data-driven).
    data.controller->setJumpSpeed(ConfigManager::get().physics.jumpPower);
    dynamicsWorld_->addAction(data.controller);

    characterControllers_[key] = data;
}

void PhysicsSystem::setEntityWalkDirection(entt::entity entity, glm::vec3 walkDisplacement) {
    auto it = characterControllers_.find(static_cast<uint32_t>(entity));
    if (it == characterControllers_.end()) return;
    it->second.controller->setWalkDirection(
        btVector3(walkDisplacement.x, walkDisplacement.y, walkDisplacement.z));
}

void PhysicsSystem::jumpCharacterController(entt::entity entity) {
    auto it = characterControllers_.find(static_cast<uint32_t>(entity));
    if (it == characterControllers_.end()) return;
    if (it->second.controller->canJump())
        it->second.controller->jump(btVector3(0.0f, 0.0f, 0.0f));
}

void PhysicsSystem::removeCharacterController(entt::entity entity) {
    auto it = characterControllers_.find(static_cast<uint32_t>(entity));
    if (it == characterControllers_.end()) return;
    auto& data = it->second;
    if (dynamicsWorld_) {
        if (data.controller)  dynamicsWorld_->removeAction(data.controller);
        if (data.ghostObject) dynamicsWorld_->removeCollisionObject(data.ghostObject);
    }
    delete data.controller;   data.controller  = nullptr;
    delete data.ghostObject;  data.ghostObject  = nullptr;
    delete data.capsuleShape; data.capsuleShape = nullptr;
    characterControllers_.erase(it);
}

bool PhysicsSystem::hasCharacterController(entt::entity entity) const {
    return characterControllers_.count(static_cast<uint32_t>(entity)) > 0;
}

void PhysicsSystem::warpCharacterController(entt::entity entity,
                                             const glm::vec3& feetPos) {
    auto it = characterControllers_.find(static_cast<uint32_t>(entity));
    if (it == characterControllers_.end()) return;
    auto& data = it->second;
    // Place the capsule centre (ghost object origin) at feetPos.y + capsuleHalfHeight
    // so the capsule bottom sits exactly at feetPos.y (terrain surface).
    btVector3 centre(feetPos.x,
                     feetPos.y + data.capsuleHalfHeight,
                     feetPos.z);
    data.controller->warp(centre);
}

// ---------------------------------------------------------------------------
// Legacy OOP convenience wrappers — client only (#ifndef HEADLESS_SERVER)
// ---------------------------------------------------------------------------

#ifndef HEADLESS_SERVER

void PhysicsSystem::addStaticBody(Entity* entity, ColliderShape shape,
                                   glm::vec3 halfExtents, float friction,
                                   float restitution) {
    if (!entity) return;
    PhysicsBodyDef def;
    def.type        = BodyType::Static;
    def.shape       = shape;
    def.mass        = 0.0f;
    def.halfExtents = halfExtents;
    def.friction    = friction;
    def.restitution = restitution;
    addDynamicBodyInternal(entity->getHandle(), def,
                           entity->getPosition(), entity->getRotation());
}

void PhysicsSystem::addDynamicBody(Entity* entity, const PhysicsBodyDef& def) {
    if (!entity) return;
    // Read initial pose directly from Entity (which mirrors TransformComponent).
    addDynamicBodyInternal(entity->getHandle(), def,
                           entity->getPosition(), entity->getRotation());
}

void PhysicsSystem::addKinematicBody(Entity* entity, const PhysicsBodyDef& def) {
    if (!entity) return;
    PhysicsBodyDef kDef = def;
    kDef.type = BodyType::Kinematic;
    addDynamicBody(entity, kDef);
}

void PhysicsSystem::removeRigidBody(Entity* entity) {
    if (!entity) return;
    removeRigidBody(entity->getHandle());
}

void PhysicsSystem::setCharacterController(Player* player,
                                            float capsuleRadius,
                                            float capsuleHeight) {
    if (!dynamicsWorld_ || !player) return;
    playerPtr_ = player;

    capsuleShape_ = new btCapsuleShape(capsuleRadius, capsuleHeight);
    capsuleHalfHeight_ = capsuleHeight * 0.5f + capsuleRadius;

    ghostObject_ = new btPairCachingGhostObject();
    btTransform t;
    t.setIdentity();
    glm::vec3 pos = player->getPosition();
    t.setOrigin(btVector3(pos.x, pos.y + capsuleHalfHeight_, pos.z));
    ghostObject_->setWorldTransform(t);
    ghostObject_->setCollisionShape(capsuleShape_);
    ghostObject_->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);

    dynamicsWorld_->addCollisionObject(
        ghostObject_,
        btBroadphaseProxy::CharacterFilter,
        btBroadphaseProxy::StaticFilter | btBroadphaseProxy::DefaultFilter);

    characterController_ = new btKinematicCharacterController(
        ghostObject_, capsuleShape_, 0.35f);
    characterController_->setGravity(dynamicsWorld_->getGravity());
    // Configure jump speed from ConfigManager (data-driven).
    characterController_->setJumpSpeed(ConfigManager::get().physics.jumpPower);
    dynamicsWorld_->addAction(characterController_);
}

void PhysicsSystem::syncCharacterToPlayer() {
    if (!ghostObject_ || !playerPtr_) return;
    const btTransform& t = ghostObject_->getWorldTransform();
    glm::vec3 pos = btToGlm(t.getOrigin());
    pos.y -= capsuleHalfHeight_;
    playerPtr_->setPosition(pos);
}

void PhysicsSystem::setPlayerWalkDirection(float vx, float vz, bool wantsJump) {
    if (!characterController_) return;
    characterController_->setWalkDirection(btVector3(vx, 0.0f, vz));
    if (wantsJump && characterController_->canJump()) {
        characterController_->jump();
    }
}

void PhysicsSystem::warpPlayer(const glm::vec3& feetPos) {
    if (!characterController_ || !ghostObject_) return;
    btVector3 centre(feetPos.x, feetPos.y + capsuleHalfHeight_, feetPos.z);
    characterController_->warp(centre);
}

void PhysicsSystem::renderDebug(const glm::mat4& view, const glm::mat4& projection) {
    if (debugDrawEnabled_ && debugDrawer_) {
        debugDrawer_->flushLines(view, projection);
    }
}

#endif // !HEADLESS_SERVER
