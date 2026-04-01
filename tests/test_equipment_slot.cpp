// tests/test_equipment_slot.cpp
// Unit tests for EquipmentSlot enum utilities.

#include <gtest/gtest.h>
#include "../src/Animation/EquipmentSlot.h"

TEST(EquipmentSlotTest, FromString_ValidNames) {
    EXPECT_EQ(equipmentSlotFromString("Head"),     EquipmentSlot::Head);
    EXPECT_EQ(equipmentSlotFromString("Torso"),    EquipmentSlot::Torso);
    EXPECT_EQ(equipmentSlotFromString("Legs"),     EquipmentSlot::Legs);
    EXPECT_EQ(equipmentSlotFromString("Hands"),    EquipmentSlot::Hands);
    EXPECT_EQ(equipmentSlotFromString("Feet"),     EquipmentSlot::Feet);
    EXPECT_EQ(equipmentSlotFromString("Cape"),     EquipmentSlot::Cape);
    EXPECT_EQ(equipmentSlotFromString("Helmet"),   EquipmentSlot::Helmet);
    EXPECT_EQ(equipmentSlotFromString("Necklace"), EquipmentSlot::Necklace);
    EXPECT_EQ(equipmentSlotFromString("Weapon"),   EquipmentSlot::Weapon);
}

TEST(EquipmentSlotTest, FromString_InvalidReturnsCount) {
    EXPECT_EQ(equipmentSlotFromString(""),           EquipmentSlot::Count);
    EXPECT_EQ(equipmentSlotFromString("head"),       EquipmentSlot::Count);  // case-sensitive
    EXPECT_EQ(equipmentSlotFromString("TORSO"),      EquipmentSlot::Count);
    EXPECT_EQ(equipmentSlotFromString("Shield"),     EquipmentSlot::Count);
    EXPECT_EQ(equipmentSlotFromString("Nonsense"),   EquipmentSlot::Count);
}

TEST(EquipmentSlotTest, ToString_ValidSlots) {
    EXPECT_STREQ(equipmentSlotToString(EquipmentSlot::Head),     "Head");
    EXPECT_STREQ(equipmentSlotToString(EquipmentSlot::Torso),    "Torso");
    EXPECT_STREQ(equipmentSlotToString(EquipmentSlot::Legs),     "Legs");
    EXPECT_STREQ(equipmentSlotToString(EquipmentSlot::Hands),    "Hands");
    EXPECT_STREQ(equipmentSlotToString(EquipmentSlot::Feet),     "Feet");
    EXPECT_STREQ(equipmentSlotToString(EquipmentSlot::Cape),     "Cape");
    EXPECT_STREQ(equipmentSlotToString(EquipmentSlot::Helmet),   "Helmet");
    EXPECT_STREQ(equipmentSlotToString(EquipmentSlot::Necklace), "Necklace");
    EXPECT_STREQ(equipmentSlotToString(EquipmentSlot::Weapon),   "Weapon");
}

TEST(EquipmentSlotTest, ToString_Count_ReturnsUnknown) {
    EXPECT_STREQ(equipmentSlotToString(EquipmentSlot::Count), "Unknown");
}

TEST(EquipmentSlotTest, RoundTrip) {
    // For every valid slot, toString → fromString should round-trip.
    for (int i = 0; i < static_cast<int>(EquipmentSlot::Count); ++i) {
        auto slot = static_cast<EquipmentSlot>(i);
        const char* name = equipmentSlotToString(slot);
        EXPECT_EQ(equipmentSlotFromString(name), slot)
            << "Round-trip failed for slot index " << i << " (\"" << name << "\")";
    }
}

TEST(EquipmentSlotTest, SlotCount_IsNine) {
    // The expected slot count (Head..Weapon).
    EXPECT_EQ(static_cast<int>(EquipmentSlot::Count), 9);
}
