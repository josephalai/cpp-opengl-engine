//
// PhysicsSystem.cpp — implementation of the Bullet Physics subsystem.
//

#include "PhysicsSystem.h"
#include "PhysicsDebugDrawer.h"
#include "../Entities/Player.h"
#include "../Input/InputMaster.h"

#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include <iostream>

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
    collisionConfig_ = new btDefaultCollisionConfiguration();
    dispatcher_      = new btCollisionDispatcher(collisionConfig_);
    broadphase_      = new btDbvtBroadphase();
    solver_          = new btSequentialImpulseConstraintSolver();
    dynamicsWorld_   = new btDiscreteDynamicsWorld(
        dispatcher_, broadphase_, solver_, collisionConfig_);

    // Match existing kGravity = -50 in Player.h
    dynamicsWorld_->setGravity(btVector3(0.0f, -50.0f, 0.0f));

    // Register the ghost-object callback needed by btKinematicCharacterController
    broadphase_->getOverlappingPairCache()->setInternalGhostPairCallback(
        new btGhostPairCallback());

    // Create and register debug drawer
    debugDrawer_ = new PhysicsDebugDrawer();
    debugDrawer_->init();
    dynamicsWorld_->setDebugDrawer(debugDrawer_);
}

void PhysicsSystem::update(float deltaTime) {
    if (!dynamicsWorld_) return;

    // F3 toggles physics debug rendering
    static bool f3WasDown = false;
    bool f3IsDown = InputMaster::isKeyDown(F3);
    if (f3IsDown && !f3WasDown) {
        debugDrawEnabled_ = !debugDrawEnabled_;
        debugDrawer_->setDebugMode(debugDrawEnabled_
            ? btIDebugDraw::DBG_DrawWireframe | btIDebugDraw::DBG_DrawAabb
            : btIDebugDraw::DBG_NoDebug);
    }
    f3WasDown = f3IsDown;

    // Sync kinematic bodies FROM entities TO physics world
    for (auto& e : entries_) {
        if (e.bodyType != BodyType::Kinematic) continue;
        glm::vec3 pos = e.entity ? e.entity->getPosition()
                                  : (e.aEntity ? e.aEntity->getPosition() : glm::vec3(0));
        glm::vec3 rot = e.entity ? e.entity->getRotation()
                                  : (e.aEntity ? e.aEntity->getRotation() : glm::vec3(0));

        glm::quat q = glm::quat(glm::radians(rot));
        btTransform t;
        t.setIdentity();
        t.setOrigin(glmToBt(pos));
        t.setRotation(btQuaternion(q.x, q.y, q.z, q.w));
        e.body->getMotionState()->setWorldTransform(t);
        e.body->setWorldTransform(t);
    }

    // Step simulation
    dynamicsWorld_->stepSimulation(deltaTime, 10);

    // Sync character controller ghost transform → player
    if (characterController_ && playerPtr_) {
        syncCharacterToPlayer();
    }

    // Sync dynamic body transforms → entities
    for (auto& e : entries_) {
        if (e.bodyType != BodyType::Dynamic) continue;
        btTransform t;
        e.body->getMotionState()->getWorldTransform(t);
        glm::vec3 pos = btToGlm(t.getOrigin());
        glm::vec3 rot = btQuatToEulerDeg(t.getRotation());
        if (e.entity) {
            e.entity->setPosition(pos);
            e.entity->setRotation(rot);
        } else if (e.aEntity) {
            e.aEntity->setPosition(pos);
            e.aEntity->setRotation(rot);
        }
    }

    // Optionally draw debug wireframes
    if (debugDrawEnabled_ && debugDrawer_) {
        dynamicsWorld_->debugDrawWorld();
    }
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
        if (characterController_) {
            dynamicsWorld_->removeAction(characterController_);
            dynamicsWorld_->removeCollisionObject(ghostObject_);
        }
    }
    entries_.clear();
    groundBodies_.clear();

    for (auto* s : shapes_)      delete s;
    for (auto* s : groundShapes_) delete s;
    shapes_.clear();
    groundShapes_.clear();

    delete characterController_; characterController_ = nullptr;
    delete ghostObject_;          ghostObject_         = nullptr;
    delete capsuleShape_;         capsuleShape_        = nullptr;

    delete debugDrawer_;  debugDrawer_ = nullptr;
    delete dynamicsWorld_; dynamicsWorld_ = nullptr;
    delete solver_;        solver_        = nullptr;
    delete broadphase_;    broadphase_    = nullptr;
    delete dispatcher_;    dispatcher_    = nullptr;
    delete collisionConfig_; collisionConfig_ = nullptr;
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
// Public API
// ---------------------------------------------------------------------------

void PhysicsSystem::addStaticBody(Entity* entity, ColliderShape shape,
                                   glm::vec3 halfExtents, float friction,
                                   float restitution) {
    if (!dynamicsWorld_ || !entity) return;
    PhysicsBodyDef def;
    def.type        = BodyType::Static;
    def.shape       = shape;
    def.mass        = 0.0f;
    def.halfExtents = halfExtents;
    def.friction    = friction;
    def.restitution = restitution;
    addDynamicBody(entity, def);  // mass=0 → static in Bullet
}

void PhysicsSystem::addDynamicBody(Entity* entity, const PhysicsBodyDef& def) {
    if (!dynamicsWorld_ || !entity) return;

    btCollisionShape* shape = createShape(def);
    shapes_.push_back(shape);

    glm::vec3 pos = entity->getPosition();
    glm::vec3 rot = entity->getRotation();
    glm::quat q   = glm::quat(glm::radians(rot));

    btTransform startTransform;
    startTransform.setIdentity();
    startTransform.setOrigin(glmToBt(pos));
    startTransform.setRotation(btQuaternion(q.x, q.y, q.z, q.w));

    btScalar mass = (def.type == BodyType::Static) ? 0.0f : def.mass;
    btVector3 localInertia(0, 0, 0);
    if (mass > 0.0f) shape->calculateLocalInertia(mass, localInertia);

    auto* motionState = new btDefaultMotionState(startTransform);
    btRigidBody::btRigidBodyConstructionInfo ci(mass, motionState, shape, localInertia);
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
    entry.entity   = entity;
    entry.body     = body;
    entry.bodyType = def.type;
    entries_.push_back(entry);
}

void PhysicsSystem::addKinematicBody(Entity* entity, const PhysicsBodyDef& def) {
    if (!entity) return;
    PhysicsBodyDef kDef = def;
    kDef.type = BodyType::Kinematic;
    addDynamicBody(entity, kDef);
}

void PhysicsSystem::removeRigidBody(Entity* entity) {
    if (!dynamicsWorld_ || !entity) return;
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
        if (it->entity == entity) {
            dynamicsWorld_->removeRigidBody(it->body);
            delete it->body->getMotionState();
            delete it->body;
            entries_.erase(it);
            return;
        }
    }
}

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

void PhysicsSystem::setCharacterController(Player* player,
                                            float capsuleRadius,
                                            float capsuleHeight) {
    if (!dynamicsWorld_ || !player) return;
    playerPtr_ = player;

    capsuleShape_ = new btCapsuleShape(capsuleRadius, capsuleHeight);

    ghostObject_ = new btPairCachingGhostObject();
    btTransform t;
    t.setIdentity();
    glm::vec3 pos = player->getPosition();
    t.setOrigin(btVector3(pos.x, pos.y, pos.z));
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
    dynamicsWorld_->addAction(characterController_);
}

void PhysicsSystem::syncCharacterToPlayer() {
    if (!ghostObject_ || !playerPtr_) return;
    const btTransform& t = ghostObject_->getWorldTransform();
    glm::vec3 pos = btToGlm(t.getOrigin());
    playerPtr_->setPosition(pos);
}

void PhysicsSystem::setPlayerWalkDirection(float vx, float vz, bool wantsJump) {
    if (!characterController_) return;
    // vx/vz are per-frame displacements (velocity × dt) supplied by Player::move().
    // btKinematicCharacterController::setWalkDirection in Bullet 3.x applies the
    // vector directly each tick without internal dt scaling, so the caller must
    // supply an appropriately-scaled value.  Calling with (0,0,0) when idle is
    // required — Bullet persists the previous walk direction otherwise and the
    // character keeps drifting.
    characterController_->setWalkDirection(btVector3(vx, 0.0f, vz));
    if (wantsJump && characterController_->canJump()) {
        characterController_->jump();
    }
}

void PhysicsSystem::renderDebug(const glm::mat4& view, const glm::mat4& projection) {
    if (debugDrawEnabled_ && debugDrawer_) {
        debugDrawer_->flushLines(view, projection);
    }
}
