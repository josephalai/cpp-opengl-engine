-- scripts/skills/woodcutting.lua
-- Resource Gathering: Woodcutting
-- Called by InteractionSystem when a player interacts with a Tree entity.
--
-- Return value convention:
--   > 0.0  → keep the action running; value is the cooldown until next swing
--   = 0.0  → action is complete; C++ removes ActionStateComponent

function on_interact(player_id, target_id, engine)
    -- Play animation on the client
    engine.Network.broadcastAnimation(player_id, "Chop_Axe")

    -- Pull stats from C++ ECS
    local woodcutting_level = engine.Stats.getLevel(player_id, "Woodcutting")

    -- Roll deterministic RNG for a log award
    if engine.Math.rollChance(0.10 + (woodcutting_level * 0.01)) then
        engine.Inventory.addItem(player_id, "Logs", 1)
        engine.Network.sendMessage(player_id, "You get some logs.")

        -- 10% chance to deplete the resource
        if engine.Math.rollChance(0.10) then
            engine.Entities.destroy(target_id)
            return 0.0  -- Return 0 to tell C++ the state machine is over
        end
    end

    -- Return the cooldown timer (OSRS 4-tick cycle = 2.4 seconds)
    return 2.4
end
