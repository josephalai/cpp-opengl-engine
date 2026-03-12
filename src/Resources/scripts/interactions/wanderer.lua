-- scripts/interactions/wanderer.lua
--
-- Stateful Wandering NPC Interaction
--
-- Behaviour contract:
--   1. The C++ engine handles WalkTo automatically (player walks to NPC).
--   2. Once in interact_range, this script fires.
--   3. engine.AI.pause() halts the NPC's C++ wander loop temporarily so the
--      NPC stands still during conversation.
--   4. After 3 normal interactions the player triggers a 45-second cooldown.
--      Any click within that window yields "Fuck off, peach fuzz." and the
--      timer resets from that moment.
--
-- State tables live at module scope inside this Sol2 isolated environment and
-- persist across calls for the lifetime of the server session.

-- Per-player interaction counters (keyed by player_id).
local interaction_counts = {}

-- Per-player timestamp of when the 45-second penalty was last imposed.
-- Uses os.time() (integer seconds since Unix epoch).
local penalty_start = {}

-- How many normal interactions before the cooldown kicks in.
local MAX_FREE_INTERACTIONS = 3

-- Duration of the penalty cooldown in seconds.
local PENALTY_DURATION = 45

-- How long the NPC stands still per interaction (seconds).
local NPC_PAUSE_DURATION = 5.0

function on_interact(player_id, target_id, engine)
    -- Freeze the NPC's C++ wander AI for NPC_PAUSE_DURATION seconds so they
    -- face the player instead of walking away mid-dialogue.
    engine.AI.pause(target_id, NPC_PAUSE_DURATION)

    -- Pivot to face the player once the NPC has halted.
    engine.Transform.lookAt(target_id, player_id)

    local now = os.time()

    -- ---- Penalty check -------------------------------------------------------
    local pstart = penalty_start[player_id]
    if pstart ~= nil and (now - pstart) < PENALTY_DURATION then
        -- Player is within the 45-second cooling-off period.
        engine.Network.sendMessage(player_id, "Wanderer: Fuck off, peach fuzz.")
        -- Reset the penalty timer from this latest click.
        penalty_start[player_id] = now
        return 0.0
    end
    -- --------------------------------------------------------------------------

    -- ---- Normal sequential dialogue ------------------------------------------
    local count = interaction_counts[player_id] or 0

    if count == 0 then
        engine.Network.sendMessage(player_id, "Wanderer: Hi there! Lovely weather we're having.")
    elseif count == 1 then
        engine.Network.sendMessage(player_id, "Wanderer: Again? Do you mind? I'm trying to walk.")
    elseif count == 2 then
        engine.Network.sendMessage(player_id, "Wanderer: STOP BOTHERING ME.")
        -- Third interaction triggers the 45-second penalty.
        penalty_start[player_id] = now
        -- Reset count so they can have the full sequence again after the penalty.
        interaction_counts[player_id] = 0
        return 0.0
    end

    interaction_counts[player_id] = count + 1
    -- --------------------------------------------------------------------------

    -- Return 0.0: the interaction completes immediately.
    return 0.0
end
