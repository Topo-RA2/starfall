return {
    clear = function(ctx)
        local args = {Target=ctx.self:position(), Range=0, PickupTypes={"MedKit","ShortLived"}}
        assert(ctx:action("RemovePickup", args))
        assert(not ctx:action("RemovePickup", args))
        ctx:action("GiveResearchPoints", {Amount=1})
    end
}
