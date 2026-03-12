-- scripts/interactions/talk.lua
-- Triggered by the C++ InteractionSystem when a player interacts with an
-- npc_wanderer or npc_guard and the server confirms the action.

function on_interact(player_id, target_id, engine)
    print("[Lua] Player " .. player_id .. " is talking to NPC " .. target_id)

    -- Send the dialogue message directly to the player's client via ENet.
    -- On the client this prints as:
    --   [NPC DIALOGUE]: Greetings, traveler! ...
    engine.Network.sendMessage(player_id, "Greetings, traveler! The C++ Engine obeys my Lua commands!")

    -- Return 0.0 to tell C++ the interaction is instant and complete.
    return 0.0
end
