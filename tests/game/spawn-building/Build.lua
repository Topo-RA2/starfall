return {
    cast = function(ctx)
        local site = {x=80, y=65}
        assert(ctx:action("SpawnBuilding", {
            BuildingType="GAPOWR", Target=site, Owner="Victim", SearchRadius=0,
        }))
        assert(not ctx:action("SpawnBuilding", {
            BuildingType="GAPOWR", Target=site, SearchRadius=0,
        }))
        assert(ctx:runAction("BuildResult", {Target=site}))
        ctx.state.done = 1
    end,
}
