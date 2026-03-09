-- scripts/ai/wander.lua
--
-- WanderAI — walk forward for 3 seconds, then turn for 1 second, repeat.
-- This is the Lua equivalent of the C++ ServerNPCManager::tickWander().
--
-- The C++ LuaScriptEngine calls:
--   WanderAI(state, dt)
-- where `state` is a mutable AIState (timer, phase, cameraYaw) and `dt` is
-- the tick delta time.  Returns an AIResult with the movement flags to apply.

function WanderAI(state, dt)
    local result = AIResult.new()
    result.deltaTime = dt

    state.timer = state.timer + dt

    if state.phase == 0 then
        -- Walk forward
        result.moveForward = true
        if state.timer >= 3.0 then
            state.timer = 0.0
            state.phase = 1
        end
    elseif state.phase == 1 then
        -- Turn (accumulate yaw)
        state.cameraYaw = state.cameraYaw + config.npcTurnSpeed * dt
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
