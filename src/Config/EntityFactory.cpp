// src/Config/EntityFactory.cpp

#include "EntityFactory.h"
#include "PrefabManager.h"
#include "ConfigManager.h"

#include "../ECS/Components/TransformComponent.h"
#include "../ECS/Components/NetworkIdComponent.h"
#include "../ECS/Components/InputStateComponent.h"
#include "../ECS/Components/InputQueueComponent.h"
#include "../ECS/Components/AIScriptComponent.h"
#include "../Physics/PhysicsSystem.h"

#ifndef HEADLESS_SERVER
#include "../ECS/Components/AssimpModelComponent.h"
#include "../ECS/Components/AnimatedModelComponent.h"
#include "../Animation/AnimationLoader.h"
#include "../Util/FileSystem.h"
#endif

#include <nlohmann/json.hpp>
#include <iostream>

// -------------------------------------------------------------------------
// spawn — create an entity from a prefab definition
// -------------------------------------------------------------------------

entt::entity EntityFactory::spawn(entt::registry& registry,
                                  const std::string& prefabId,
                                  const glm::vec3& position,
                                  PhysicsSystem* physics) {
    const auto& prefab = PrefabManager::get().getPrefab(prefabId);
    if (prefab.is_null()) {
        std::cerr << "[EntityFactory] Unknown prefab: " << prefabId << "\n";
        return entt::null;
    }

    entt::entity entity = registry.create();

    // --- TransformComponent (always added) ---
    auto& tc = registry.emplace<TransformComponent>(entity);
    tc.position = position;

    // --- NetworkIdComponent (if "model_type" is present) ---
    if (prefab.contains("model_type")) {
        auto& nid = registry.emplace<NetworkIdComponent>(entity);
        nid.modelType = prefab["model_type"].get<std::string>();
        nid.isNPC     = prefab.value("is_npc", true);  // default true for prefab-spawned
    }

    // --- InputStateComponent (use prefab overrides or ConfigManager defaults) ---
    {
        auto& isc = registry.emplace<InputStateComponent>(entity);
        const auto& cfg = ConfigManager::get().physics;
        isc.runSpeed  = cfg.defaultRunSpeed;
        isc.turnSpeed = cfg.defaultTurnSpeed;

        // Per-entity overrides from the prefab's "components" block.
        if (prefab.contains("components")) {
            const auto& comps = prefab["components"];
            if (comps.contains("InputStateComponent")) {
                const auto& isc_json = comps["InputStateComponent"];
                isc.runSpeed  = isc_json.value("run_speed",  isc.runSpeed);
                isc.turnSpeed = isc_json.value("turn_speed", isc.turnSpeed);
            }
        }
    }

    // --- InputQueueComponent (always added for entities that need ticking) ---
    registry.emplace<InputQueueComponent>(entity);

    // --- Physics character controller ---
    if (physics && prefab.contains("physics")) {
        const auto& phys = prefab["physics"];
        float radius = phys.value("radius",
                           ConfigManager::get().physics.defaultCapsuleRadius);
        float height = phys.value("height",
                           ConfigManager::get().physics.defaultCapsuleHeight);
        physics->addCharacterController(entity, radius, height);
    }

    // --- AIScriptComponent (if "ai_script" is declared) ---
    if (prefab.contains("ai_script")) {
        auto& ai = registry.emplace<AIScriptComponent>(entity);
        ai.scriptPath = prefab["ai_script"].get<std::string>();
        // Also read the logical script name from the AIComponent block.
        if (prefab.contains("components") &&
            prefab["components"].contains("AIComponent") &&
            prefab["components"]["AIComponent"].contains("script")) {
            ai.scriptName = prefab["components"]["AIComponent"]["script"].get<std::string>();
        }
    }

#ifndef HEADLESS_SERVER
    // --- Client-side rendering ---
    // If the prefab declares a "mesh" path, attach a visual component.
    // Animated prefabs get AnimatedModelComponent (skinned-mesh rendering via
    // AnimatedRenderer); static prefabs get AssimpModelComponent (rigid rendering
    // via MasterRenderer).  The server build compiles out this entire block.
    if (prefab.contains("mesh")) {
        const std::string meshPath = prefab["mesh"].get<std::string>();

        if (prefab.value("animated", false)) {
            // --- Animated entity: load skeleton + clips, attach AnimatedModelComponent ---
            // Resolve mesh path relative to src/Resources/ (e.g. "models/player.glb").
            const std::string absPath = FileSystem::Scene(meshPath);
            AnimatedModel* animModel  = AnimationLoader::load(absPath);
            if (animModel) {
                auto normalizeClipName = [](const std::string& raw) -> std::string {
                    std::string lower(raw);
                    for (auto& c : lower)
                        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (lower.find("idle") != std::string::npos) return "Idle";
                    if (lower.find("walk") != std::string::npos) return "Walk";
                    if (lower.find("run")  != std::string::npos) return "Run";
                    if (lower.find("jump") != std::string::npos) return "Jump";
                    std::string out = raw;
                    if (!out.empty())
                        out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
                    return out;
                };

                auto* controller = new AnimationController();
                std::string firstNormName;
                bool idleFound = false;
                for (auto& clip : animModel->clips) {
                    const std::string normName = normalizeClipName(clip.name);
                    controller->addState(normName, &clip);
                    if (firstNormName.empty()) firstNormName = normName;
                    if (!idleFound && normName == "Idle") {
                        controller->setState("Idle");
                        idleFound = true;
                    }
                }
                // Fall back to first clip if no Idle clip was found.
                if (!idleFound && !firstNormName.empty())
                    controller->setState(firstNormName);

                auto& amc       = registry.emplace<AnimatedModelComponent>(entity);
                amc.model       = animModel;
                amc.controller  = controller;
                amc.ownsModel   = true;
                amc.isLocalPlayer = false;  // marked true by Engine after initial load
                // Optional per-prefab visual scale / offset.
                amc.scale = prefab.value("scale", 1.0f);
                if (prefab.contains("components") &&
                    prefab["components"].contains("AnimatedModelComponent")) {
                    const auto& j = prefab["components"]["AnimatedModelComponent"];
                    amc.scale = j.value("scale", amc.scale);
                    if (j.contains("model_offset")) {
                        amc.modelOffset.x = j["model_offset"].value("x", 0.0f);
                        amc.modelOffset.y = j["model_offset"].value("y", 0.0f);
                        amc.modelOffset.z = j["model_offset"].value("z", 0.0f);
                    }
                }
            } else {
                std::cerr << "[EntityFactory] Failed to load animated model: "
                          << absPath << "\n";
            }
        } else {
            // --- Static mesh: attach AssimpModelComponent ---
            // The mesh pointer is filled later by the asset loader when the
            // entity first enters the render view.
            auto& amc    = registry.emplace<AssimpModelComponent>(entity);
            amc.position = position;
            amc.meshPath = meshPath;
        }
    }
#endif

    return entity;
}
