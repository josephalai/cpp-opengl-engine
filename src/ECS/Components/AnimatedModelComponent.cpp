// src/ECS/Components/AnimatedModelComponent.cpp
// Runtime equipment API for the Modular Skinned Character Equipment System.
// Guarded by HEADLESS_SERVER — the server build doesn't render or manage equipment.

#ifndef HEADLESS_SERVER

#include "AnimatedModelComponent.h"
#include "../../Animation/AnimationLoader.h"
#include "../../Animation/EquipmentSlot.h"
#include <iostream>

// ---------------------------------------------------------------------------
// equipPart — load + remap a new equipment mesh and store it in the slot
// ---------------------------------------------------------------------------
void AnimatedModelComponent::equipPart(EquipmentSlot slot,
                                        const std::string& assetPath,
                                        bool hidesNaked) {
    if (!isModular || !model) {
        std::cerr << "[AnimatedModelComponent::equipPart] Cannot equip: "
                  << (isModular ? "model is null" : "entity is not modular") << "\n";
        return;
    }
    const int idx = static_cast<int>(slot);
    if (idx < 0 || idx >= static_cast<int>(EquipmentSlot::Count)) return;

    // Clean up existing equipped part in this slot
    if (equippedArmor[idx]) {
        equippedArmor[idx]->cleanUp();
        delete equippedArmor[idx];
        equippedArmor[idx] = nullptr;
    }

    auto meshes = AnimationLoader::loadModularPart(assetPath, model->skeleton);
    if (meshes.empty()) {
        std::cerr << "[AnimatedModelComponent::equipPart] No meshes loaded from '"
                  << assetPath << "'\n";
        return;
    }

    auto* part       = new ModularMeshPart();
    part->assetPath  = assetPath;
    part->slot       = slot;
    part->meshes     = std::move(meshes);
    part->hidesNakedPart = hidesNaked;
    equippedArmor[idx] = part;
}

// ---------------------------------------------------------------------------
// unequipPart — remove equipment from a slot (naked geometry becomes visible)
// ---------------------------------------------------------------------------
void AnimatedModelComponent::unequipPart(EquipmentSlot slot) {
    const int idx = static_cast<int>(slot);
    if (idx < 0 || idx >= static_cast<int>(EquipmentSlot::Count)) return;

    if (equippedArmor[idx]) {
        equippedArmor[idx]->cleanUp();
        delete equippedArmor[idx];
        equippedArmor[idx] = nullptr;
    }
}

// ---------------------------------------------------------------------------
// setNakedParts — batch-load naked body parts
// ---------------------------------------------------------------------------
void AnimatedModelComponent::setNakedParts(
    const std::vector<std::pair<EquipmentSlot, std::string>>& parts)
{
    if (!model) return;
    for (const auto& [slot, assetPath] : parts) {
        const int idx = static_cast<int>(slot);
        if (idx < 0 || idx >= static_cast<int>(EquipmentSlot::Count)) continue;

        // Clean up existing naked part in this slot
        if (nakedParts[idx]) {
            nakedParts[idx]->cleanUp();
            delete nakedParts[idx];
            nakedParts[idx] = nullptr;
        }

        auto meshes = AnimationLoader::loadModularPart(assetPath, model->skeleton);
        if (meshes.empty()) {
            std::cerr << "[AnimatedModelComponent::setNakedParts] No meshes from '"
                      << assetPath << "'\n";
            continue;
        }

        auto* part       = new ModularMeshPart();
        part->assetPath  = assetPath;
        part->slot       = slot;
        part->meshes     = std::move(meshes);
        part->hidesNakedPart = false;  // naked parts don't hide themselves
        nakedParts[idx]  = part;
    }
}

// ---------------------------------------------------------------------------
// buildActiveMeshes — assemble the per-frame draw list
// ---------------------------------------------------------------------------
std::vector<const AnimatedMesh*> AnimatedModelComponent::buildActiveMeshes() const {
    std::vector<const AnimatedMesh*> active;

    for (int i = 0; i < static_cast<int>(EquipmentSlot::Count); ++i) {
        if (equippedArmor[i]) {
            // Equipped armour — render the armour meshes.
            for (const auto& m : equippedArmor[i]->meshes)
                active.push_back(&m);

            // If the armour hides the naked part, skip it; otherwise add both.
            if (!equippedArmor[i]->hidesNakedPart && nakedParts[i]) {
                for (const auto& m : nakedParts[i]->meshes)
                    active.push_back(&m);
            }
        } else if (nakedParts[i]) {
            // No armour — render naked geometry.
            for (const auto& m : nakedParts[i]->meshes)
                active.push_back(&m);
        }
    }

    return active;
}

// ---------------------------------------------------------------------------
// cleanUpModularParts — release all modular mesh GL resources
// ---------------------------------------------------------------------------
void AnimatedModelComponent::cleanUpModularParts() {
    for (int i = 0; i < static_cast<int>(EquipmentSlot::Count); ++i) {
        if (nakedParts[i]) {
            nakedParts[i]->cleanUp();
            delete nakedParts[i];
            nakedParts[i] = nullptr;
        }
        if (equippedArmor[i]) {
            equippedArmor[i]->cleanUp();
            delete equippedArmor[i];
            equippedArmor[i] = nullptr;
        }
    }
}

#endif // HEADLESS_SERVER
