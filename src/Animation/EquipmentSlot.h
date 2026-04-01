// src/Animation/EquipmentSlot.h
// Equipment slot identifiers for the modular skinned character system.

#ifndef ENGINE_EQUIPMENTSLOT_H
#define ENGINE_EQUIPMENTSLOT_H

#include <string>

enum class EquipmentSlot {
    Head,
    Torso,
    Legs,
    Hands,
    Feet,
    Cape,
    Helmet,
    Necklace,
    Weapon,
    Count
};

/// Convert a slot name string (e.g. "Head", "Torso") to EquipmentSlot.
/// Returns EquipmentSlot::Count for unrecognised names.
inline EquipmentSlot equipmentSlotFromString(const std::string& name) {
    if (name == "Head")     return EquipmentSlot::Head;
    if (name == "Torso")    return EquipmentSlot::Torso;
    if (name == "Legs")     return EquipmentSlot::Legs;
    if (name == "Hands")    return EquipmentSlot::Hands;
    if (name == "Feet")     return EquipmentSlot::Feet;
    if (name == "Cape")     return EquipmentSlot::Cape;
    if (name == "Helmet")   return EquipmentSlot::Helmet;
    if (name == "Necklace") return EquipmentSlot::Necklace;
    if (name == "Weapon")   return EquipmentSlot::Weapon;
    return EquipmentSlot::Count;
}

/// Convert an EquipmentSlot to its string name.
inline const char* equipmentSlotToString(EquipmentSlot slot) {
    switch (slot) {
        case EquipmentSlot::Head:     return "Head";
        case EquipmentSlot::Torso:    return "Torso";
        case EquipmentSlot::Legs:     return "Legs";
        case EquipmentSlot::Hands:    return "Hands";
        case EquipmentSlot::Feet:     return "Feet";
        case EquipmentSlot::Cape:     return "Cape";
        case EquipmentSlot::Helmet:   return "Helmet";
        case EquipmentSlot::Necklace: return "Necklace";
        case EquipmentSlot::Weapon:   return "Weapon";
        default:                      return "Unknown";
    }
}

#endif // ENGINE_EQUIPMENTSLOT_H
