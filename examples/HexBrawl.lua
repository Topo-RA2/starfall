-- Independent, synchronized bomb sequences. All continuation state is serialized
-- in task data; no unit handle is needed after the planted bomb explodes.
local skill = {}

function skill.burst(ctx, data)
    local point = ctx:randomPoint({center = data.center, radius = ctx.params.radius})
    if point then
        ctx:action("DetonateWarhead", {
            Target = point,
            Warhead = ctx.params.warhead,
            Amount = data.damage
        })
    end
    data.remaining = data.remaining - 1
    if data.remaining > 0 then
        ctx:schedule(ctx.params.delay, "burst", data)
    end
end

function skill.fireworks(ctx)
    if not ctx.point or ctx.damage <= 0 then return end
    ctx:schedule(ctx.params.delay, "burst", {
        center = ctx.point,
        damage = math.max(1, math.floor(ctx.damage * ctx.params.damagefactor + 0.5)),
        remaining = ctx.params.shots
    })
end

function skill.legacy(ctx)
    local point = ctx.event.selfPoint
    if not point then return end
    for _, unit in ipairs(ctx:nearbyUnits(point, 3, {houses={"Self", "Allies"}, types={"@Infantry"}})) do
        ctx:action("Heal", {Target=unit, Amount=40})
        ctx:action("GrantPassive", {Target=unit, Passive="SF_HX_Legacy_Buff", Duration=75})
    end
end

function skill.agile_dodge(ctx)
    if not ctx:action("BlockDamage", {Target=ctx.self}) then return end
    if ctx.state.teleportPending or ctx.frame < (ctx.state.nextTeleport or 0) then return end
    -- Relocation happens after the native damage stack unwinds, never inside it.
    local task = ctx:schedule(1, "agile_teleport", {attacker=ctx.target, owner=ctx.self:owner()})
    if task then ctx.state.teleportPending = task end
end

function skill.agile_teleport(ctx, data)
    ctx.state.teleportPending = nil
    if not ctx.self or not ctx.self:alive() or ctx.self:owner() ~= data.owner
        or not ctx.self:hasPassive("SF_HX_AgileReflexes_P") then return end
    if ctx.frame < (ctx.state.nextTeleport or 0) then return end
    local origin = ctx.self:position()
    if not origin then return end
    local best, distance
    for _, unit in ipairs(ctx:nearbyUnits(ctx.self, ctx.params.range, {
        houses={"Enemies"}, types={"@GroundVehicle"}, visibleOnly=true
    })) do
        local point = unit:position()
        if point then
            if data.attacker and unit == data.attacker then best = unit; break end
            local d = (point.x-origin.x)^2 + (point.y-origin.y)^2
            if not distance or d < distance then best = unit; distance = d end
        end
    end
    if not best then return end
    if ctx:action("ChronoTeleport", {Target=ctx.self, Destination=best:position(), SearchRadius=ctx.params.searchradius, Instant=true}) then
        ctx.state.nextTeleport = ctx.frame + ctx.params.cooldown
    end
end

function skill.scavenge(ctx)
    local point = ctx.event.selfPoint
    if not point then return end
    local nearby = ctx:nearbyUnits(point, 3, {houses={"Self", "Allies"}, types={"BRUTE"}})
    local survivors = {}
    for _, unit in ipairs(nearby) do
        if unit ~= ctx.self then survivors[#survivors + 1] = unit end
    end
    if #survivors == 0 then return end
    local unit = survivors[ctx:randomInt(1, #survivors)]
    ctx:action("ChangeMaxHealth", {Target=unit, Amount=50})
    ctx:action("ChangeScale", {Target=unit, Percent=10})
    ctx:action("Heal", {Target=unit, Amount=unit:maxHealth()})
end

return skill
