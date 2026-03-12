-- scripts/interactions/guard.lua
--
-- Stateful NPC Guard Interaction
--
-- The guard waits for the player to walk within interact_range (handled by
-- the C++ InteractionSystem / WalkTo infrastructure — no Lua needed for that).
-- Once in range, this script fires and delivers sequential dialogue lines.
-- Each click advances the player's personal dialogue state independently.
--
-- The interaction state is stored in the Lua environment which persists for
-- the lifetime of the server (Sol2 isolated environment per script path).
-- See docs/SCRIPTING_GUIDE.md for full architecture overview.

-- Per-player dialogue index (keyed by player_id).
-- Persists between calls because this is a module-level variable.
local dialogue_index = {}

-- Ordered list of dialogue lines the guard will say in sequence.
local lines = {
    "Halt! Who goes there?",
    "I've got my eye on you, traveler.",
    "Move along, citizen. Nothing to see here.",
    "I used to be an adventurer like you, then I took an arrow in the knee.",
    "The guard stares at you blankly and says nothing."
}

function on_interact(player_id, target_id, engine)
    -- Face the player before speaking so the NPC makes eye contact.
    engine.Transform.lookAt(target_id, player_id)

    -- Look up this player's current position in the dialogue sequence.
    -- Defaults to line 1 on the first interaction.
    local idx = dialogue_index[player_id] or 1

    local msg = lines[idx] or lines[#lines]
    engine.Network.sendMessage(player_id, "Guard: " .. msg)

    -- Advance to the next line (stops advancing after the last line).
    if idx < #lines then
        dialogue_index[player_id] = idx + 1
    end

    -- Return 0.0: the interaction is instant and complete.
    -- The C++ state machine releases the player to IDLE.
    return 0.0
end
