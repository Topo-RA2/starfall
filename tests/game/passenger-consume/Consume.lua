return {
    consume = function(ctx)
        local candidates = ctx:nearbyUnits(ctx.self, 3, {houses={"Self"}, types={"SNIPE"}})
        assert(#candidates == 1)
        local material = candidates[1]
        assert(ctx:action("ConsumeTarget", {Target=material}))
        assert(not material:alive())
        assert(not ctx:action("ConsumeTarget", {Target=material}))
        assert(ctx:action("GiveResearchPoints", {Target=ctx.self, Amount=3}))
    end
}
