return {
    orders = function(ctx)
        if ctx.self:type() == "E1" then
            assert(ctx.self:deploy())
            return
        end
        local tank = ctx.self
        local targets = ctx:nearbyUnits(tank, 12, {houses={"Enemies"}, types={"HTK"}})
        assert(#targets == 1 and not tank:deploy())
        local destination = {x=56, y=69}
        assert(not tank:setDroneGuardCell(destination))
        assert(not targets[1]:moveTo(destination))
        assert(tank:moveTo(destination))
        ctx:schedule(65, "attackUnit", {destination=destination, enemy=targets[1]})
    end,
    attackUnit = function(ctx, data)
        local position = ctx.self:position()
        assert(position.x == data.destination.x and position.y == data.destination.y)
        assert(ctx.self:attack(data.enemy))
        ctx.state.attackIssued = true
        ctx:schedule(65, "attackGround", {})
    end,
    attackGround = function(ctx)
        assert(ctx.self:attack({x=55, y=70}))
        ctx.state.groundIssued = true
    end,
    interceptedOrder = function(ctx)
        ctx.self:attack(ctx.target)
    end,
    eat = function(ctx)
        local enemy = ctx.target
        assert(enemy:alive() and ctx.self:passengers() == 1)
        assert(ctx:actions {
            { Action = "ConsumePassenger", Target = ctx.self },
            { Action = "StealPassive", Target = "Target", Passive = { "CargoBuff" }, Amount = 2 },
        })
        assert(ctx.self:stack("CargoBuff") == 2 and enemy:alive())
        assert(ctx:action("ApplyIronCurtain", { Target = ctx.self, Duration = 600 }))
        assert(not ctx:action("ApplyIronCurtain", { Target = ctx.self, Duration = 10 }))
        ctx:runAction("Reward", { Target = ctx.self })
    end,
    damage = function(ctx)
        local own = ctx:house()
        local target = ctx:house(ctx.target:owner())
        assert(own and own.index == ctx.self:owner())
        assert(own.powerOutput == ctx.params.output and own.powerDrain == ctx.params.drain)
        assert(own.powerBalance == ctx.params.output - ctx.params.drain)
        assert(own.credits == ctx.params.credits and own.researchPoints == ctx.params.points)
        assert(target and target.index == ctx.target:owner() and target.index ~= own.index)
        assert(target.credits == ctx.params.enemycredits and target.researchPoints == ctx.params.enemypoints)
        assert(ctx:house(-1) == nil and ctx:house(999999) == nil)
        local matches = ctx:nearbyUnits(ctx.target, 5, { houses = { "Self" } })
        local found = false
        for _, unit in ipairs(matches) do
            assert(unit:owner() == ctx.self:owner())
            found = found or unit == ctx.self
        end
        assert(found)
        local amount = math.floor(ctx.damage / 2)
        ctx:action("SetDamage", { Amount = amount })
        assert(ctx.damage == amount)
    end,
    broken = function(ctx)
        error("deliberate fixture error")
    end,
}
