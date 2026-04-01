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

    std::cout << "[AnimatedModelComponent::equipPart] Equipped slot "
              << idx << " with '" << assetPath << "' ("
              << part->meshes.size() << " mesh(es), hidesNaked="
              << (hidesNaked ? "yes" : "no") << ").\n";
    std::cout << "[AnimatedModelComponent::equipPart]   Slot " << idx
              << " (" << equipmentSlotToString(slot) << "): "
              << part->meshes.size() << " mesh(es) loaded, "
              << "master skeleton bones=" << model->skeleton.getBoneCount() << ".\n";
}

// ---------------------------------------------------------------------------
// unequipPart — remove equipment from a slot (naked geometry becomes visible)
// ---------------------------------------------------------------------------
void AnimatedModelComponent::unequipPart(EquipmentSlot slot) {
    const int idx = static_cast<int>(slot);
    if (idx < 0 || idx >= static_cast<int>(EquipmentSlot::Count)) return;

    if (equippedArmor[idx]) {
        std::cout << "[AnimatedModelComponent::unequipPart] Removing slot " << idx
                  << " ('" << equippedArmor[idx]->assetPath << "').\n";
        equippedArmor[idx]->cleanUp();
        delete equippedArmor[idx];
        equippedArmor[idx] = nullptr;
    }
    else {
        std::cout << "[AnimatedModelComponent::unequipPart] Slot " << idx
                  << " (" << equipmentSlotToString(slot) << ") was already empty.\n";
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
        std::cout << "[AnimatedModelComponent::setNakedParts] Slot " << idx
                  << " loaded from '" << assetPath << "' ("
                  << part->meshes.size() << " mesh(es)).\n";
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

    // One-shot summary — prints only once per entity lifetime.
    if (!activeMeshesLoggedOnce_) {
        std::cout << "[AnimatedModelComponent::buildActiveMeshes] Slot breakdown:\n";
        for (int i = 0; i < static_cast<int>(EquipmentSlot::Count); ++i) {
            auto slot = static_cast<EquipmentSlot>(i);
            const char* name = equipmentSlotToString(slot);
            bool hasArmor = (equippedArmor[i] != nullptr);
            bool hasNaked = (nakedParts[i] != nullptr);
            if (hasArmor || hasNaked) {
                std::cout << "  [" << name << "] armor="
                          << (hasArmor ? std::to_string(equippedArmor[i]->meshes.size()) + " mesh(es)" : "none")
                          << ", naked="
                          << (hasNaked ? std::to_string(nakedParts[i]->meshes.size()) + " mesh(es)" : "none")
                          << (hasArmor && equippedArmor[i]->hidesNakedPart ? " (naked hidden)" : "")
                          << "\n";
            }
        }
    }

    // One-shot summary — prints only once per entity lifetime.
    if (!activeMeshesLoggedOnce_) {
        std::cout << "[AnimatedModelComponent::buildActiveMeshes] "
                  << active.size() << " active mesh(es) from "
                  << static_cast<int>(EquipmentSlot::Count) << " slot(s).\n";
        activeMeshesLoggedOnce_ = true;
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
