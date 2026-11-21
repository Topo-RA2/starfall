#include "../../../src/Starfall.cpp"

namespace {
void checkPickup(bool ok,const char* label) {
    std::ofstream("ground-pickups.log",std::ios::app)<<(ok?"PASS ":"FAIL ")<<label<<"\n";
    if(!ok) TerminateProcess(GetCurrentProcess(),2);
}
}
void checkCrateAvoidance() {
    auto& map=MapClass::Instance;
    CellStruct origin=CellStruct::Empty;
    for(short y=50;y<70 && origin==CellStruct::Empty;++y) for(short x=50;x<70;++x) {
        CellStruct at{x,y};
        auto cell=map.TryGetCellAt(at);
        if(cell && map.IsWithinUsableArea(at,true) && !cell->GetContent() && cell->OverlayTypeIndex==-1 &&
            map.NearByLocation(at,SpeedType::Track,-1,MovementZone::Normal,false,1,1,false,false,false,true,CellStruct::Empty,false,false)==at) {
            origin=at;break;
        }
    }
    checkPickup(origin!=CellStruct::Empty,"found native crate test cell");
    auto cell=map.TryGetCellAt(origin);
    checkPickup(placePickup("medkit",origin) && groundPickups.contains({origin.X,origin.Y}),"pickup placed first");
    auto placeNativeOverlay=reinterpret_cast<bool(__thiscall*)(CellStruct*)>(0x4A18F0);
    checkPickup(!placeNativeOverlay(&origin) && !hasNativeCrate(cell),"random refresh rejects occupied final cell");
    checkPickup(map.PlacePowerupCrate(origin,Powerup::Veteran) && !hasNativeCrate(cell),"targeted crate avoids pickup");
    CellClass* crate=nullptr;
    for(int dy=-3;dy<=3;++dy) for(int dx=-3;dx<=3;++dx) {
        auto candidate=map.TryGetCellAt(CellStruct{short(origin.X+dx),short(origin.Y+dy)});
        if(candidate && hasNativeCrate(candidate) && candidate->OverlayData==static_cast<unsigned>(Powerup::Veteran)) crate=candidate;
    }
    checkPickup(crate && groundPickups.contains({origin.X,origin.Y}),"relocated crate retains reward and pickup remains");
    auto at=crate->MapCoords;
    checkPickup(placePickup("shortlived",at) && groundPickups.size()==2 && !groundPickups.contains({at.X,at.Y}) &&
        hasNativeCrate(crate),"pickup avoids existing native crate");
    for(int dy=-3;dy<=3;++dy) for(int dx=-3;dx<=3;++dx)
        groundPickups.try_emplace({short(origin.X+dx),short(origin.Y+dy)},GroundPickup{"medkit",0});
    checkPickup(!map.PlacePowerupCrate(origin,Powerup::Veteran) && !hasNativeCrate(cell),"full neighborhood rejects targeted crate");

    Event event{};
    auto clear=[&](const char* id,std::optional<CellStruct> point=std::nullopt) {
        return apply(skills.at(id).actions.front(),event,nullptr,Objects{},1,std::nullopt,point);
    };
    checkPickup(!skills.contains("clearwrong") && !skills.contains("clearinvalid"),"reject invalid cleanup type");
    auto queued=pending.size();
    checkPickup(clear("clearcell",origin) && !clear("clearcell",origin),"default cleanup matches exact cell once");
    PickupCell edge{short(origin.X+3),short(origin.Y+4)},outside{short(origin.X+4),short(origin.Y+4)};
    groundPickups.emplace(edge,GroundPickup{"medkit",0});
    groundPickups.emplace(outside,GroundPickup{"medkit",0});
    checkPickup(clear("clearkits",origin) && !groundPickups.contains(edge) && groundPickups.contains(outside) &&
        groundPickups.size()==2,"circular cleanup includes boundary and filters type");
    checkPickup(clear("clearall") && groundPickups.empty() && !clear("clearall") &&
        pending.size()==queued && hasNativeCrate(crate),"global cleanup without center preserves native crate and queues no reward");
}
void checkPickupDrawing() {
    MapClass::Instance.Reveal(HouseClass::CurrentPlayer);
    auto surface=DSurface::Temp;
    auto bounds=surface->GetRect();
    for(short y=0;y<128;++y) for(short x=0;x<128;++x) {
        auto cell=MapClass::Instance.TryGetCellAt(CellStruct{x,y});
        if(!cell || cell->GetContent() || cell->IsShrouded() || cell->IsFogged() ||
           !MapClass::Instance.IsWithinUsableArea(CellStruct{x,y},false)) continue;
        auto [pt,visible]=TacticalClass::Instance->CoordsToClient(cell->GetCoordsWithBridge());
        if(!visible || pt.X<50 || pt.Y<50 || pt.X+50>=bounds.Width || pt.Y+50>=bounds.Height) continue;
        auto pixels=[&] {
            vector<int> values;
            for(int dy=-40;dy<=40;++dy) for(int dx=-40;dx<=40;++dx) {
                Point2D p{pt.X+dx,pt.Y+dy};values.push_back(surface->GetPixel(&p));
            }
            return values;
        };
        groundPickups.emplace(PickupCell{x,y},GroundPickup{"medkit",unsigned(Unsorted::CurrentFrame)});
        RectangleStruct background{pt.X-40,pt.Y-40,81,81};
        surface->FillRect(&background,0x39e7);
        auto before=pixels();drawGroundPickups();auto drawn=pixels();
        groundPickups.erase({x,y});cell->MarkForRedraw();
        if(before!=drawn) {checkPickup(true,"visible SHP changes ground pixels");return;}
    }
    checkPickup(false,"visible SHP changes ground pixels");
}
DEFINE_HOOK(0x55B4EB,SF_TestGroundPickups,5) {
    if(GetFileAttributesA("Starfall.test")==INVALID_FILE_ATTRIBUTES) return 0;
    int f=Unsorted::CurrentFrame;
    if(f==20 && GetFileAttributesA("reload.flag")==INVALID_FILE_ATTRIBUTES) {
        Research::ensureReady();
        checkPickup(pickupTypes.contains("medkit") && !pickupTypes.contains("invalidreward") && skills.contains("dropkit"),"pickup validation");
        auto house=HouseClass::FindByPlayerAt(HouseClass::PlayerAtA);
        UnitClass* source=nullptr;
        for(auto u:UnitClass::Array) if(u->Owner==house && alive(u)) {source=u;break;}
        auto fallback=resolvePickupImage("");
        checkPickup(fallback && fallback->Frames>0,"default pickup art available");
        checkPickupDrawing();
        auto other=HouseClass::FindByPlayerAt(HouseClass::PlayerAtB);
        auto tank=static_cast<UnitClass*>(UnitTypeClass::Find("MTNK")->CreateObject(other));
        auto cell=MapClass::Instance.TryGetCellAt(CellStruct{48,65});
        checkPickup(source && tank && cell && tank->Unlimbo(cell->GetCoords(),DirType::North),"enemy placed");
        tank->Health=20;tank->ForceMission(Mission::Guard);
        auto overlay=cell->OverlayTypeIndex;
        grantToUnit(source,PassiveGrant{{"dropkit"},-1});
        capture("onkill",source,tank,source,tank,0);dispatch();
        checkPickup(groundPickups.contains({48,65}) && cell->OverlayTypeIndex==overlay,"event spawns independent pickup");
        checkPickup(placePickup("shortlived",{48,65}) && groundPickups.size()==2,"occupied cell uses nearby slot");
        checkPickup(pickupImage("piff.shp")!=nullptr,"custom SHP loads");
        std::ofstream("pickup.ids")<<tank->UniqueID<<" "<<Research::balance(other->ArrayIndex);
        checkPickup(ScenarioClass::SaveGame(GetModuleHandleA("CnCNet-Spawner.dll")?"Pickups.sav":"Saved Games\\Pickups.sav",L"Pickups"),"saved occupied and expiring pickups");
    }
    if(f>=24) {
        DWORD id=0;int before=0;std::ifstream("pickup.ids")>>id>>before;
        UnitClass* tank=nullptr;for(auto u:UnitClass::Array) if(u->UniqueID==id) tank=u;
        checkPickup(tank && tank->Health==45 && Research::balance(tank->Owner->ArrayIndex)==before+7,"collector receives rewards exactly once");
        checkPickup(groundPickups.empty(),"collected and expired removed");
        checkPickup(placePickup("shortlived",tank->GetCell()->MapCoords),"cell reusable");
        collectPickups();dispatch();
        checkPickup(Research::balance(tank->Owner->ArrayIndex)==before+21,"random actions execute per draw");
        checkCrateAvoidance();
        auto at=tank->GetCell()->MapCoords;
        checkPickup(placePickup("medkit",at),"place pickup for Lua");
        auto snapshot=eventSnapshot("ondeploy",tank,nullptr,nullptr,nullptr,0);
        Objects objects{{tank->UniqueID,tank}};
        checkPickup(apply(skills.at("scriptcleanup").actions.front(),snapshot,tank,objects,1) &&
            groundPickups.empty() && Research::balance(tank->Owner->ArrayIndex)==before+22,"Lua point cleanup reports success and empty result");
        ScriptFields fields{{"action","RemovePickup"},{"action.target","Point"}};
        Skill step{};step.id="clearpoint";step.trigger="onsuperweapon";
        checkPickup(placePickup("medkit",at) && ActionSequence::prepare(fields) && readAction(step,step.id,&fields),"prepare superweapon cleanup");
        snapshot.hasImpact=true;snapshot.impact=tank->GetCell()->GetCoordsWithBridge();
        checkPickup(apply(step.actions.front(),snapshot,nullptr,objects,1) && groundPickups.empty(),"superweapon cleanup uses clicked cell");
        TerminateProcess(GetCurrentProcess(),0);
    }
    return 0;
}
