-- scripts/skills/woodcutting.lua
-- Resource Gathering: Woodcutting
-- Called by InteractionSystem when a player interacts with a Tree entity.

local BASE_SUCCESS_RATE  = 0.10   -- Base chance to get a log per swing
local LEVEL_BONUS        = 0.01   -- Additional chance per Woodcutting level
local DEPLETION_CHANCE   = 0.10   -- Chance the tree is felled after a successful chop
local SWING_COOLDOWN     = 2.4    -- Seconds between swings (OSRS 4-tick cycle)

function on_interact(player_id, target_id, engine)
    -- Play animation on the client
    engine.Network.broadcastAnimation(player_id, "Chop_Axe")

    -- Pull stats from C++ ECS
    local woodcutting_level = engine.Stats.getLevel(player_id, "Woodcutting")

    -- Roll deterministic RNG
    if engine.Math.rollChance(BASE_SUCCESS_RATE + (woodcutting_level * LEVEL_BONUS)) then
        engine.Inventory.addItem(player_id, "Logs", 1)
        engine.Network.sendMessage(player_id, "You get some logs.")

        -- Chance to deplete the resource
        if engine.Math.rollChance(DEPLETION_CHANCE) then
            engine.Entities.destroy(target_id)
            return 0.0  -- Return 0 to tell C++ the state machine is over
        end
    end

    -- Return the cooldown timer to C++
    return SWING_COOLDOWN
end
