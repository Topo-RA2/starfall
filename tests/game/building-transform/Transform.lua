local skill = {}
function skill.cast(ctx)
    local self = ctx.self
    ctx.state.original = self
    ctx.state.id = self:id()
    ctx.state.count = 1
    ctx.state.anim = ctx:spawnAnim("PIFF", self:position(), {duration=150, loop=true})
    assert(ctx.state.anim)
    assert(ctx:schedule(10, "afterFirst", {unit=self}))
    assert(ctx:schedule(35, "again", {unit=self}))
    assert(not ctx:action("Transform", {UnitType="GAREFN"}))
    assert(ctx:action("Transform", {UnitType="NAHAND"}))
    assert(self:alive() and self:type() == "NAHAND" and self:id() == ctx.state.id)
    assert(self == ctx.state.original and self:hasPassive("TemporaryBuilding"))
    assert(self:stack("TemporaryBuilding") == 2)
    assert(ctx:action("Heal", {Target=self, Amount=1}))
    ctx.state.count = ctx.state.count + 1
end
function skill.afterFirst(ctx, data)
    assert(ctx.state.count == 2 and ctx.self == data.unit and ctx.self == ctx.state.original)
    assert(ctx.self:type() == "NAHAND" and ctx.self:hasPassive("TemporaryBuilding"))
    assert(ctx:moveAnim(ctx.state.anim, ctx.self:position(), 5))
    ctx.state.count = 3
end
function skill.again(ctx, data)
    assert(ctx.state.count == 3 and ctx.self == data.unit and ctx.self:id() == ctx.state.id)
    assert(ctx:action("Transform", {Target=data.unit, UnitType="GAPOWR"}))
    assert(ctx.self:type() == "GAPOWR" and ctx.self:hasPassive("TemporaryBuilding"))
    ctx.state.count = 4
    assert(ctx:removeAnim(ctx.state.anim))
    assert(ctx:schedule(5, "last", {unit=data.unit}))
end
function skill.last(ctx, data)
    assert(ctx.self == data.unit and ctx.state.count == 4)
    ctx.state.count = 5
end
return skill
