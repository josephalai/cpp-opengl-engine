-- scripts/skills/woodcutting.lua
-- Resource Gathering: Woodcutting
--
-- Called by InteractionSystem when a player interacts with a Tree entity.
-- The C++ engine handles WalkTo automatically — no Lua needed for that.
--
-- Behaviour per tick:
--   1. Plays the chop animation.
--   2. Prints "CHOP CHOP CHOP..." so the player sees feedback each swing.
--   3. Rolls a 25% base success chance (modified by Woodcutting level).
--   4. On success, awards logs and tells the player.
--   5. Returns 2.4 to loop every 2.4 seconds (OSRS 4-tick cycle).
--   6. If the player presses WASD or clicks another target, C++ cancels the
--      loop immediately (ActionInterruption in ServerMain.cpp).

local BASE_SUCCESS_RATE = 0.25   -- 25% base chance per swing
local LEVEL_BONUS       = 0.01   -- +1% per Woodcutting level
local DEPLETION_CHANCE  = 0.10   -- 10% chance the tree falls after a successful chop
local SWING_COOLDOWN    = 2.4    -- Seconds between swings (OSRS 4-tick cycle)

function on_interact(player_id, target_id, engine)
    -- Play the chop animation on all clients.
    engine.Network.broadcastAnimation(player_id, "Chop_Axe")

    -- Each swing: tell the player something is happening.
    engine.Network.sendMessage(player_id, "CHOP CHOP CHOP...")

    -- Pull current Woodcutting level from C++ ECS stats.
    local woodcutting_level = engine.Stats.getLevel(player_id, "Woodcutting")

    -- Roll deterministic RNG against the success threshold.
    if engine.Math.rollChance(BASE_SUCCESS_RATE + (woodcutting_level * LEVEL_BONUS)) then
        engine.Inventory.addItem(player_id, "Logs", 1)
        engine.Network.sendMessage(player_id, "You successfully chopped the wood!")

        -- Small chance the tree is fully felled and removed from the world.
        if engine.Math.rollChance(DEPLETION_CHANCE) then
            engine.Entities.destroy(target_id)
            -- Returning 0 stops the loop — tree is gone.
            return 0.0
        end
    end

    -- Return the cooldown in seconds.  The C++ Interaction System will hold
    -- the player in ACTION state and re-call this function after SWING_COOLDOWN.
    -- If the player presses WASD or right-clicks something else, C++ cancels
    -- the ActionStateComponent and this function is never called again.
    return SWING_COOLDOWN
end

