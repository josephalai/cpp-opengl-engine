-- scripts/interactions/banker_dialogue.lua
-- NPC Dialogue: Banker
-- Called by InteractionSystem when a player interacts with a Banker NPC.
--
-- Return value convention:
--   = 0.0  → instant action; C++ removes ActionStateComponent immediately.
--             The UI remains open on the client until the player closes it.

function on_interact(player_id, target_id, engine)
    -- Snap player rotation to face the NPC
    engine.Transform.lookAt(player_id, target_id)

    -- Send packet to open the specific client-side UI
    engine.Network.sendOpenUI(player_id, "UI_BANKER_DIALOGUE")

    -- Dialogue is an instant action.  Returning 0.0 instantly un-assigns
    -- the ActionStateComponent in C++.
    return 0.0
end
