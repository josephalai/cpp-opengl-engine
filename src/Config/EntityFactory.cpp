// src/Config/EntityFactory.cpp

#include "EntityFactory.h"
#include "PrefabManager.h"
#include "ConfigManager.h"

#include "../ECS/Components/TransformComponent.h"
#include "../ECS/Components/NetworkIdComponent.h"
#include "../ECS/Components/InputStateComponent.h"
#include "../ECS/Components/InputQueueComponent.h"
#include "../ECS/Components/AIScriptComponent.h"
#include "../ECS/Components/InteractableComponent.h"
#include "../Physics/PhysicsSystem.h"

#ifndef HEADLESS_SERVER
#include "../ECS/Components/AssimpModelComponent.h"
#include "../ECS/Components/AnimatedModelComponent.h"
#include "../ECS/Components/AnimationControllerComponent.h"
#include "../Animation/AnimationLoader.h"
#include "../Util/FileSystem.h"
#include <glm/gtc/matrix_transform.hpp>
#endif

#include <nlohmann/json.hpp>
#include <iostream>

// -------------------------------------------------------------------------
// spawn — create an entity from a prefab definition
// -------------------------------------------------------------------------

entt::entity EntityFactory::spawn(entt::registry& registry,
                                  const std::string& prefabId,
                                  const glm::vec3& position,
                                  PhysicsSystem* physics,
                                  const glm::vec3& rotation,
                                  float scale) {
    const auto& prefab = PrefabManager::get().getPrefab(prefabId);
    if (prefab.is_null()) return entt::null;

    entt::entity entity = registry.create();

    auto& tc = registry.emplace<TransformComponent>(entity);
    tc.position = position;
    tc.rotation = rotation;
    tc.scale = scale;

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
        
        // Default to character_controller if it's a legacy player/npc without a type.
        std::string physType = phys.value("type", "character_controller"); 

        if (physType == "static" || physType == "dynamic" || physType == "kinematic") {
            PhysicsBodyDef def;
            std::string shapeStr = phys.value("shape", "box");
            if (shapeStr == "sphere")  def.shape = ColliderShape::Sphere;
            else if (shapeStr == "capsule") def.shape = ColliderShape::Capsule;
            else def.shape = ColliderShape::Box;

            def.mass        = phys.value("mass", 0.0f);
            def.friction    = phys.value("friction", 0.5f);
            def.restitution = phys.value("restitution", 0.3f);
            
            // Scale physics bounds to match visual scale
            def.radius = phys.value("radius", 0.5f) * tc.scale;
            def.height = phys.value("height", 1.8f) * tc.scale;

            if (phys.contains("halfExtents") && phys["halfExtents"].is_array() && phys["halfExtents"].size() >= 3) {
                def.halfExtents = glm::vec3(
                    phys["halfExtents"][0].get<float>(),
                    phys["halfExtents"][1].get<float>(),
                    phys["halfExtents"][2].get<float>()
                ) * tc.scale;
            } else {
                def.halfExtents = glm::vec3(0.5f) * tc.scale;
            }

            if (physType == "dynamic") {
                def.type = BodyType::Dynamic;
                physics->addDynamicBody(entity, def);
            } else if (physType == "kinematic") {
                def.type = BodyType::Kinematic;
                physics->addKinematicBody(entity, def);
            } else {
                def.type = BodyType::Static;
                physics->addStaticBody(entity, def.shape, def.halfExtents, def.friction, def.restitution);
            }
        } else {
            // "character_controller"
            float capRadius = phys.value("radius", ConfigManager::get().physics.defaultCapsuleRadius);
            float capHeight = phys.value("height", ConfigManager::get().physics.defaultCapsuleHeight);
            physics->addCharacterController(entity, capRadius, capHeight);
        }
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

    // --- InteractableComponent (if prefab contains an "InteractableComponent" block) ---
    // Supports both top-level and nested-under-"components" declarations.
    // Precedence: top-level takes priority over "components" block.
    // New prefabs should use top-level placement (e.g., tree.json).
    {
        const nlohmann::json* icJson = nullptr;
        if (prefab.contains("InteractableComponent"))
            icJson = &prefab["InteractableComponent"];
        else if (prefab.contains("components") &&
                 prefab["components"].contains("InteractableComponent"))
            icJson = &prefab["components"]["InteractableComponent"];

        if (icJson) {
            auto& ic = registry.emplace<InteractableComponent>(entity);
            ic.scriptPath    = icJson->value("script",         "");
            ic.interactRange = icJson->value("interact_range", 1.5f);
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
            // ---------------------------------------------------------------
            // Animated entity — two load scenarios:
            //
            // Scenario 1 (Monolithic): No "AnimationController" component, or
            //   its "animations" object is missing/empty.  Use the legacy
            //   AnimationLoader::load() that reads mesh, skeleton and embedded
            //   clips from a single .glb file.
            //
            // Scenario 2 (Modular): "AnimationController.animations" contains
            //   external file paths.  Call loadSkin() for the mesh/skeleton,
            //   then loadExternalAnimation() for each clip, and store the
            //   resulting clips in an AnimationControllerComponent.
            // ---------------------------------------------------------------

            // Determine which scenario applies.
            const nlohmann::json* acJson = nullptr;  // AnimationController block
            bool modularMode = false;
            if (prefab.contains("components") &&
                prefab["components"].contains("AnimationController")) {
                const auto& ac = prefab["components"]["AnimationController"];
                if (ac.contains("animations") && ac["animations"].is_object() &&
                    !ac["animations"].empty()) {
                    acJson      = &prefab["components"]["AnimationController"];
                    modularMode = true;
                }
            }

            const std::string absPath  = FileSystem::Scene(meshPath);

            AnimatedModel* animModel = nullptr;
            if (modularMode) {
                // ----------------------------------------------------------
                // Scenario 2: load skin + external animations
                // ----------------------------------------------------------

                // Resolve optional mesh_path override inside AnimatedModelComponent block
                std::string skinAbsPath = absPath;
                if (prefab.contains("components") &&
                    prefab["components"].contains("AnimatedModelComponent")) {
                    const auto& amcJson = prefab["components"]["AnimatedModelComponent"];
                    if (amcJson.contains("mesh_path")) {
                        skinAbsPath = FileSystem::Scene(
                            amcJson["mesh_path"].get<std::string>());
                    }
                }

                animModel = AnimationLoader::loadSkin(skinAbsPath);
                if (!animModel) {
                    std::cerr << "[EntityFactory] loadSkin failed: " << skinAbsPath << "\n";
                }
            } else {
                // ----------------------------------------------------------
                // Scenario 1: monolithic load (legacy path)
                // ----------------------------------------------------------
                animModel = AnimationLoader::load(absPath);
            }

            if (!animModel) {
                std::cerr << "[EntityFactory] Failed to load animated model: "
                          << absPath << "\n";
                // Fall through; entity is created without a visual component.
            } else {
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

                // Resolve optional animation_map from prefab top-level or from
                // components.AnimatedModelComponent.animation_map.
                // (Only used in Scenario 1 — Scenario 2 uses "AnimationController.animations".)
                const nlohmann::json* animMapJson = nullptr;
                if (!modularMode) {
                    if (prefab.contains("animation_map") &&
                        prefab["animation_map"].is_object()) {
                        animMapJson = &prefab["animation_map"];
                    } else if (prefab.contains("components") &&
                               prefab["components"].contains("AnimatedModelComponent")) {
                        const auto& j = prefab["components"]["AnimatedModelComponent"];
                        if (j.contains("animation_map") && j["animation_map"].is_object())
                            animMapJson = &j["animation_map"];
                    }
                }

                auto* controller = new AnimationController();
                std::string firstStateName;
                bool idleFound = false;

                if (modularMode) {
                    // ----------------------------------------------------------
                    // Scenario 2: load each external animation clip and register
                    // it with the AnimationController state machine.
                    // ----------------------------------------------------------
                    const auto& animsJson = (*acJson)["animations"];
                    std::string defaultState = acJson->value("default_state", "");

                    // We'll build the AnimationControllerComponent alongside
                    auto& acc = registry.emplace<AnimationControllerComponent>(entity);
                    acc.defaultState = defaultState;

                    for (auto& [stateName, animPathVal] : animsJson.items()) {
                        const std::string animRelPath = animPathVal.get<std::string>();
                        const std::string animAbsPath = FileSystem::Scene(animRelPath);

                        auto clip = AnimationLoader::loadExternalAnimation(
                            animAbsPath, &animModel->skeleton);
                        if (!clip) {
                            std::cerr << "[EntityFactory] loadExternalAnimation failed for state '"
                                      << stateName << "': " << animAbsPath << "\n";
                            continue;
                        }

                        acc.animations[stateName] = clip;
                        controller->addState(stateName, clip.get());

                        if (firstStateName.empty()) firstStateName = stateName;
                        if (!idleFound && stateName == "Idle") {
                            controller->setState("Idle");
                            idleFound = true;
                        }
                    }

                    // Apply default_state if explicitly specified and not already set.
                    if (!idleFound && !defaultState.empty() &&
                        acc.animations.count(defaultState)) {
                        controller->setState(defaultState);
                        idleFound = true;
                    }
                    acc.currentAnimationName = controller->getCurrentStateName();

                } else if (animMapJson) {
                    // Scenario 1 with explicit animation_map
                    for (auto& [stateName, clipNameVal] : animMapJson->items()) {
                        const std::string clipName = clipNameVal.get<std::string>();
                        AnimationClip* foundClip = nullptr;
                        for (auto& clip : animModel->clips) {
                            if (clip.name == clipName) { foundClip = &clip; break; }
                        }
                        if (foundClip) {
                            controller->addState(stateName, foundClip);
                            if (firstStateName.empty()) firstStateName = stateName;
                            if (!idleFound && stateName == "Idle") {
                                controller->setState("Idle");
                                idleFound = true;
                            }
                        } else {
                            std::cerr << "[EntityFactory] animation_map entry '"
                                      << stateName << "' references clip '"
                                      << clipName << "' which was not found in '"
                                      << meshPath << "'\n";
                        }
                    }
                } else {
                    // Scenario 1 with auto-detection
                    for (auto& clip : animModel->clips) {
                        const std::string normName = normalizeClipName(clip.name);
                        controller->addState(normName, &clip);
                        if (firstStateName.empty()) firstStateName = normName;
                        if (!idleFound && normName == "Idle") {
                            controller->setState("Idle");
                            idleFound = true;
                        }
                    }
                }

                // Fall back to first clip if no Idle / default_state clip was found.
                if (!idleFound && !firstStateName.empty())
                    controller->setState(firstStateName);

                auto& amc       = registry.emplace<AnimatedModelComponent>(entity);
                amc.model       = animModel;
                amc.controller  = controller;
                amc.ownsModel   = true;
                amc.isLocalPlayer = false;  // marked true by Engine after initial load
                // Default the model-space correction to the loader's auto-detected
                // coordinateCorrection.  The prefab can override it with model_rotation.
                amc.modelRotationMat = animModel->coordinateCorrection;
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
                    if (j.contains("model_rotation")) {
                        const float rx = j["model_rotation"].value("x", 0.0f);
                        const float ry = j["model_rotation"].value("y", 0.0f);
                        const float rz = j["model_rotation"].value("z", 0.0f);
                        glm::mat4 rotMat(1.0f);
                        rotMat = glm::rotate(rotMat, glm::radians(rx), glm::vec3(1.0f, 0.0f, 0.0f));
                        rotMat = glm::rotate(rotMat, glm::radians(ry), glm::vec3(0.0f, 1.0f, 0.0f));
                        rotMat = glm::rotate(rotMat, glm::radians(rz), glm::vec3(0.0f, 0.0f, 1.0f));
                        amc.modelRotationMat = rotMat;
                    }
                }
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
