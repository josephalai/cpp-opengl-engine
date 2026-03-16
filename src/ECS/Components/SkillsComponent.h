// src/ECS/Components/SkillsComponent.h
//
// Phase 5 — Server-authoritative character skill progression.
// Attached to every player entity on the headless server.  Raw XP integers are
// the single source of truth; levels are always computed on demand from XP so
// that the XP curve can be updated without a migration.

#ifndef ECS_SKILLSCOMPONENT_H
#define ECS_SKILLSCOMPONENT_H

#include "../../Network/NetworkPackets.h"
#include <cstdint>
#include <array>

// ---------------------------------------------------------------------------
// Skill identifier enum — index into SkillsComponent::xp[]
// Keep in sync with Network::kSkillCount (23).
// ---------------------------------------------------------------------------
enum class SkillId : int {
    Attack       = 0,
    Defence      = 1,
    Strength     = 2,
    Hitpoints    = 3,
    Ranged       = 4,
    Prayer       = 5,
    Magic        = 6,
    Cooking      = 7,
    Woodcutting  = 8,
    Fletching    = 9,
    Fishing      = 10,
    Firemaking   = 11,
    Crafting     = 12,
    Smithing     = 13,
    Mining       = 14,
    Herblore     = 15,
    Agility      = 16,
    Thieving     = 17,
    Slayer       = 18,
    Farming      = 19,
    Runecraft    = 20,
    Hunter       = 21,
    Construction = 22,
};

/// Human-readable skill name from its integer ID.
inline const char* skillName(int id) {
    static constexpr const char* kNames[] = {
        "Attack", "Defence", "Strength", "Hitpoints", "Ranged",
        "Prayer", "Magic", "Cooking", "Woodcutting", "Fletching",
        "Fishing", "Firemaking", "Crafting", "Smithing", "Mining",
        "Herblore", "Agility", "Thieving", "Slayer", "Farming",
        "Runecraft", "Hunter", "Construction"
    };
    if (id < 0 || id >= Network::kSkillCount) return "Unknown";
    return kNames[id];
}

// ---------------------------------------------------------------------------
// OSRS XP level thresholds.  Index 0 = level 1 threshold = 0 XP.
// ---------------------------------------------------------------------------
inline uint32_t osrsLevelThreshold(int level) {
    static constexpr uint32_t kThresholds[99] = {
        0,       83,      174,     276,     388,     512,     650,     801,
        969,     1154,    1358,    1584,    1833,    2107,    2411,    2746,
        3115,    3523,    3973,    4470,    5018,    5624,    6291,    7028,
        7842,    8740,    9730,    10824,   12031,   13363,   14833,   16456,
        18247,   20224,   22406,   24815,   27473,   30408,   33648,   37224,
        41171,   45529,   50339,   55649,   61512,   67983,   75127,   83014,
        91721,   101333,  111945,  123660,  136594,  150872,  166636,  184040,
        203254,  224466,  247886,  273742,  302288,  333804,  368599,  407015,
        449428,  496254,  547953,  605032,  668051,  737627,  814445,  898470,
        990733,  1092097, 1200772, 1318051, 1444295, 1580019, 1726006, 1882701,
        2051199, 2231535, 2423803, 2628895, 2847256, 3079316, 3325517, 3586299,
        3862110, 4153402, 4460663, 4784389, 5125096, 5483312, 5859594, 6254516,
        6668658
    };
    if (level < 1)  return 0;
    if (level > 99) level = 99;
    return kThresholds[level - 1];
}

/// Returns the skill level [1, 99] for a raw XP total.
inline int levelFromXp(uint32_t rawXp) {
    for (int lvl = 99; lvl >= 1; --lvl) {
        if (rawXp >= osrsLevelThreshold(lvl)) return lvl;
    }
    return 1;
}

/// Returns XP remaining until the next level (0 at level 99).
inline uint32_t xpToNextLevel(uint32_t rawXp) {
    int lvl = levelFromXp(rawXp);
    if (lvl >= 99) return 0;
    uint32_t next = osrsLevelThreshold(lvl + 1);
    return (next > rawXp) ? (next - rawXp) : 0;
}

/// Server-side skills/XP state for a player entity.
struct SkillsComponent {
    static constexpr int kCount = Network::kSkillCount;

    std::array<uint32_t, kCount> xp{};  ///< Raw XP per skill.

    /// Add XP to a skill.  Returns the new XP total.
    uint32_t addXp(SkillId skill, uint32_t amount) {
        int idx = static_cast<int>(skill);
        if (idx < 0 || idx >= kCount) return 0;
        xp[idx] += amount;
        return xp[idx];
    }

    /// Build a wire-ready SkillsSyncPacket.
    Network::SkillsSyncPacket toSyncPacket() const {
        Network::SkillsSyncPacket pkt{};
        for (int i = 0; i < kCount; ++i) pkt.xp[i] = xp[i];
        return pkt;
    }
};

#endif // ECS_SKILLSCOMPONENT_H
