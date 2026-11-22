local reward = {}
function reward.grant(ctx)
    assert(ctx:action("GrantRandomResearch", {Rarity="Insane", Target=ctx.target}))
    assert(not ctx:action("GrantRandomResearch", {Rarity="Insane", Target=ctx.target}))
    assert(ctx:action("GrantFreeResearch", {Target=ctx.target}))
    assert(not ctx:action("GrantFreeResearch", {ResearchOptions={"WrongCountry","Unknown"}, Target=ctx.target}))
    ctx.state.checked = 1
end
return reward
