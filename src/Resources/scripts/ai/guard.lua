-- scripts/ai/guard.lua
--
-- GuardAI — stand still, occasionally look around.
-- This is the Lua equivalent of the C++ ServerNPCManager::tickGuard().
--
-- The C++ LuaScriptEngine calls:
--   GuardAI(state, dt)
-- where `state` is a mutable AIState (timer, phase, cameraYaw) and `dt` is
-- the tick delta time.  Returns an AIResult with the movement flags to apply.

function GuardAI(state, dt)
    local result = AIResult.new()
    result.deltaTime = dt

    state.timer = state.timer + dt

    if state.phase == 0 then
        -- Stand still
        if state.timer >= 4.0 then
            state.timer = 0.0
            state.phase = 1
        end
    elseif state.phase == 1 then
        -- Look left
        state.cameraYaw = state.cameraYaw + config.npcTurnSpeed * dt
        if state.timer >= 0.5 then
            state.timer = 0.0
            state.phase = 2
        end
    elseif state.phase == 2 then
        -- Look right
        state.cameraYaw = state.cameraYaw - config.npcTurnSpeed * dt
        if state.timer >= 1.0 then
            state.timer = 0.0
            state.phase = 0
        end
    else
        state.phase = 0
        state.timer = 0.0
    end

    result.cameraYaw = state.cameraYaw
    return result
end
