return {
    check = function(ctx)
        assert(not ctx:actions({
            {Action="RemoveAttachedUnit", Slot="Left", UnitType="MTNK"},
            {Action="GiveCredits", Amount=500}
        }))
        assert(ctx:action("RemoveAttachedUnit", {UnitType="E1", Count=2}))
        assert(not ctx:action("RemoveAttachedUnit", {Slot="Right", Count="All"}))
        assert(ctx:action("AttachUnit", {UnitType="E1", Slot="Aux"}))
        assert(ctx:action("RemoveAttachedUnit", {Slot="Aux", Count=7}))
        assert(not ctx:action("RemoveAttachedUnit", {Slot="Aux"}))
        assert(ctx:action("GiveCredits", {Amount=1}))
    end
}
