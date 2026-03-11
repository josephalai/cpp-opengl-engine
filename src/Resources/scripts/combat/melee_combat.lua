-- scripts/combat/melee_combat.lua
-- Combat: Melee Attack
-- Called by InteractionSystem when a player interacts with a hostile NPC.

function on_interact(player_id, target_id, engine)
    engine.Network.broadcastAnimation(player_id, "Slash_Sword")

    local player_stats = engine.Stats.getAll(player_id)
    local target_stats = engine.Stats.getAll(target_id)

    -- Execute combat formula
    local damage = engine.CombatMath.calculateMeleeHit(player_stats, target_stats)

    -- Apply damage to target's ECS HealthComponent
    engine.Health.dealDamage(target_id, damage)

    -- Broadcast the floating red damage number to nearby clients
    engine.Network.broadcastDamageSplat(target_id, damage)

    if engine.Health.isDead(target_id) then
        engine.Loot.generateDrop(target_id, "goblin_drop_table")
        engine.Entities.destroy(target_id)
        return 0.0  -- Combat over
    end

    -- Dynamically return cooldown based on equipped weapon speed
    local weapon_speed = engine.Equipment.getWeaponSpeed(player_id)
    return weapon_speed
end
