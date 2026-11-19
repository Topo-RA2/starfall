local skill = {}
function skill.cast(ctx)
    ctx.state.casts = (ctx.state.casts or 0) + 1
    ctx.state.roll = ctx:randomInt(-1000000, 1000000)
    if ctx.state.casts == 1 then
        assert(not ctx:action("SpawnBuilding", {BuildingType="GAPOWR", Target=ctx.point, SearchRadius=0}))
        assert(ctx:action("SpawnBuilding", {BuildingType="GAPOWR", Target=ctx.point, SearchRadius=4}))
    end
    assert(ctx:action("GiveCredits", {Amount=37}))
    assert(ctx:action("ForceMove", {
        Target=ctx.target, Offset={x=0.25,y=0}, Height=2, AirborneHeight=1, Duration=24, IgnoreCollision=false, Tilt=false,
    }))
    local anim = ctx:spawnAnim("PIFF", ctx.point, {loop=true,duration=35})
    assert(anim and ctx:moveAnim(anim, {x=ctx.point.x+1,y=ctx.point.y}, 20))
    ctx:schedule(5, "pulse", {left=4})
end
function skill.pulse(ctx, data)
    ctx.state.pulses = (ctx.state.pulses or 0) + 1
    ctx.state.roll = ctx:randomInt(-1000000, 1000000)
    if ctx.state.pulses == 1 then
        local plants = ctx:nearbyUnits(ctx.point or ctx.target:position(), 8, {houses={"Self"}, types={"GAPOWR"}})
        assert(#plants == 1)
        local plant = plants[1]
        ctx.state.building = plant
        ctx.state.buildingId = plant:id()
        assert(ctx:action("GrantPassive", {Target=plant, Passive="MP_Cloak", Duration=120}))
        assert(ctx:action("Transform", {Target=plant, UnitType="NAHAND"}))
        assert(plant:alive() and plant:type() == "NAHAND" and plant:id() == ctx.state.buildingId)
    elseif ctx.state.pulses == 4 then
        local plant = ctx.state.building
        assert(plant:alive() and plant:hasPassive("MP_Cloak"))
        assert(ctx:action("Transform", {Target=plant, UnitType="GAPOWR"}))
        assert(plant:id() == ctx.state.buildingId and plant:type() == "GAPOWR")
    end
    assert(ctx:action("Damage", {Target=ctx.target,Amount=3}))
    if data.left > 1 then ctx:schedule(5, "pulse", {left=data.left-1}) end
end
return skill
