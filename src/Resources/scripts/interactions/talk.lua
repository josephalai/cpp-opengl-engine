-- scripts/interactions/talk.lua
-- Triggered by the C++ InteractionSystem when a player walks within
-- interact_range of an npc_wanderer and the server confirms the action.

function on_interact(player_id, target_id, engine)
    print("[Lua] Player " .. player_id .. " is interacting with NPC " .. target_id)

    -- In the future, this will open a Dialogue GUI!
    -- engine.Network.sendOpenUI(player_id, "UI_NPC_DIALOGUE")

    -- Return 0.0 to tell C++ the interaction is instant and over.
    -- (Returning a positive value would cause C++ to wait that many seconds
    -- before executing this script again for a repeating interaction.)
    return 0.0
end
