return {
    onDetonate = function(ctx)
        if ctx.damage ~= 17 then return end
        assert(ctx.warhead == "SA" and ctx.self == nil and ctx.target == nil and ctx.superWeapon == nil)
        assert(ctx.point and ctx.owner ~= nil and ctx.castId > 0)
        assert(ctx:action("DetonateWarhead", {Target=ctx.point, Warhead=ctx.warhead, Amount=1}))
        assert(ctx:action("GiveResearchPoints", {Amount=ctx.params.bonus}))
    end,
    onFire = function(ctx)
        assert(ctx.self:alive())
        assert(ctx.point.x == ctx.event.targetPoint.x and ctx.point.y == ctx.event.targetPoint.y)
        ctx.state.count = (ctx.state.count or 0) + 1
        ctx.state.target = ctx.target and ctx.target:id() or 0
        ctx.state.x = ctx.point.x
        ctx.state.y = ctx.point.y
        ctx:schedule(6, "after", {x = ctx.point.x, y = ctx.point.y})
    end,
    after = function(ctx, data)
        ctx.state.finished = (ctx.state.finished or 0) + 1
        ctx.state.lastX = data.x
        ctx.state.lastY = data.y
        if ctx.state.finished == 1 then
            assert(ctx:action("FireWeapon", {Weapon="105mm", Target=data}))
            ctx.state.followUp = 1
        end
    end
}
