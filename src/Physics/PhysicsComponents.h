//
// PhysicsComponents.h — lightweight component-like structs for attaching
// Bullet physics data to existing OOP entities.
//

#ifndef ENGINE_PHYSICSCOMPONENTS_H
#define ENGINE_PHYSICSCOMPONENTS_H

#include "glm/glm.hpp"

enum class BodyType { Static, Dynamic, Kinematic };
enum class ColliderShape { Box, Sphere, Capsule, Mesh };

struct PhysicsBodyDef {
    BodyType      type        = BodyType::Dynamic;
    ColliderShape shape       = ColliderShape::Box;
    float         mass        = 1.0f;
    glm::vec3     halfExtents = glm::vec3(0.5f);  ///< for Box
    float         radius      = 0.5f;              ///< for Sphere/Capsule
    float         height      = 1.8f;              ///< for Capsule
    float         friction    = 0.5f;
    float         restitution = 0.3f;
};

#endif // ENGINE_PHYSICSCOMPONENTS_H
