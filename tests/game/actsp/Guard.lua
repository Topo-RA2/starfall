return {
    assign = function(ctx)
        local unit = ctx.self
        if unit:type() == "ASGNDPOST" then
            assert(unit:setDroneGuardCell({x=88, y=90}))
        elseif unit:type() == "GAPILL" then
            assert(unit:setDroneGuardCell({x=71, y=82}))
        elseif unit:type() == "BFRT" then
            assert(unit:setDroneGuardCell({x=85, y=71}))
        else
            assert(not unit:setDroneGuardCell({x=71, y=82}))
        end
        ctx.state.checked = true
    end
}
