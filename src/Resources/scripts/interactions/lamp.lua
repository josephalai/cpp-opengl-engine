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
    "Am I bright enough?",
    "I've got my light on you, traveler.",
    "Move along, citizen. Don't stare at me.",
    "I used to blink. Now I do not.",
    "The lamp stares at you blankly and says nothing. You realize that it was just a lamp all along."
}

function on_interact(player_id, target_id, engine)
    -- Face the player before speaking so the NPC makes eye contact.
    engine.Transform.lookAt(target_id, player_id)

    -- Look up this player's current position in the dialogue sequence.
    -- Defaults to line 1 on the first interaction.
    local idx = dialogue_index[player_id] or 1

    local msg = lines[idx] or lines[#lines]
    engine.Network.sendMessage(player_id, "Lamp: " .. msg)

    -- Advance to the next line (stops advancing after the last line).
    if idx < #lines then
        dialogue_index[player_id] = idx + 1
    end

    -- Return 0.0: the interaction is instant and complete.
    -- The C++ state machine releases the player to IDLE.
    return 0.0
end
