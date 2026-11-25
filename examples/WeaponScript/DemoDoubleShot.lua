return {
    onFire = function(ctx)
        ctx:action("FireWeapon", {Weapon="105mm", Target=ctx.target or ctx.point})
        ctx:schedule(6, "followUp", ctx.point)
    end,

    followUp = function(ctx, originalCell)
        ctx:action("FireWeapon", {Weapon="105mm", Target=originalCell})
    end
}
