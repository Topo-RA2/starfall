#include "../../../src/Starfall.cpp"

namespace {
void check(bool ok,const char* message) {
    std::ofstream("spawn-building.log",std::ios::app)<<(ok?"PASS ":"FAIL ")<<message<<'\n';
    if(!ok) TerminateProcess(GetCurrentProcess(),2);
}
CoordStruct coords(CellStruct cell) {return MapClass::Instance.GetCellAt(cell)->GetCoords();}
BuildingClass* buildingAt(CellStruct cell) {return MapClass::Instance.GetCellAt(cell)->GetBuilding();}
int plants() {
    int count=0;
    for(auto b:BuildingClass::Array) if(b && !_stricmp(b->Type->ID,"GAPOWR")) ++count;
    return count;
}
bool spawn(BuildingTypeClass* type,HouseClass* owner,CellStruct cell,int radius=0) {
    return BuildingSpawn::create(type,owner,coords(cell),radius);
}
TechnoClass* fixture(TechnoTypeClass* type,HouseClass* owner,CellStruct cell) {
    auto unit=static_cast<TechnoClass*>(type->CreateObject(owner));
    check(unit && unit->Unlimbo(coords(cell),DirType::North),"place fixture");
    unit->ForceMission(Mission::Sleep);
    return unit;
}
bool parse(ScriptFields fields) {
    Skill s{};s.id="buildcheck";s.trigger="onfire";
    return readAction(s,s.id,&fields);
}
void config() {
    const ScriptFields good{{"action","spawnbuilding"},{"action.buildingtype","gapowr"}};
    check(skills.contains("buildondeploy") && skills.contains("luabuild") && parse(good),"INI configuration");
    for(auto id:{"", "MTNK", "missing_building", "GAPLUG1", "GASAND"}) {
        auto f=good;f["action.buildingtype"]=id;
        check(!parse(f),"reject unsupported building type");
    }
    for(auto radius:{"-1","33","1.5","bad","1,2"}) {
        auto f=good;f["action.searchradius"]=radius;
        check(!parse(f),"reject invalid radius");
    }
    for(auto radius:{"0","32"}) {
        auto f=good;f["action.searchradius"]=radius;
        check(parse(f),"accept radius bounds");
    }
    auto f=good;f["action.unittype"]="MTNK";check(!parse(f),"reject wrong type field");
    f=good;f["action.destination"]="50,65";check(!parse(f),"reject teleport destination field");
    f=good;f["action.owner"]="Player9";check(!parse(f),"reject invalid owner");
    f=good;f["action"]="spawnunit";f["action.unittype"]="MTNK";check(!parse(f),"reject building field on other action");
    f={{"action","spawnunit"},{"action.unittype","GAPOWR"}};
    check(!parse(f),"SpawnUnit still rejects buildings");
    f={{"action","spawnunit"},{"action.unittypes","E1,MTNK"},{"action.weights","0,5"},{"action.randomcount","3"}};
    check(parse(f),"random spawn configuration");
    auto invalid=f;invalid["action.weights"]="0,0";check(!parse(invalid),"random spawn rejects zero weights");
    invalid=f;invalid["action.weights"]="1";check(!parse(invalid),"random spawn rejects weight mismatch");
    invalid=f;invalid["action.unittype"]="E1";check(!parse(invalid),"random spawn rejects mixed types");
    invalid=f;invalid["action.randomcount"]="0";check(!parse(invalid),"random spawn rejects invalid count");
    f=good;f["action.target"]="Point";f["action.owner"]="Owner";
    check(ActionSequence::prepare(f) && parse(f),"superweapon point and owner aliases");
}
void terrain(BuildingTypeClass* type,HouseClass* owner) {
    auto& map=MapClass::Instance;
    CellStruct anchor{68,65};
    check(BuildingSpawn::available(type,owner,anchor),"terrain fixture initially clear");
    auto cell=map.GetCellAt(CellStruct{69,66});
    auto flags=cell->Flags;cell->Flags|=CellFlags::BridgeHead;
    check(!spawn(type,owner,anchor),"reject bridge in distant foundation cell");cell->Flags=flags;
    auto slope=cell->SlopeIndex;cell->SlopeIndex=1;
    check(!spawn(type,owner,anchor),"reject slope in distant foundation cell");cell->SlopeIndex=slope;
    auto level=cell->Level;++cell->Level;
    check(!spawn(type,owner,anchor),"reject uneven foundation");cell->Level=level;
    auto land=cell->LandType;cell->LandType=LandType::Water;
    check(!spawn(type,owner,anchor),"reject incompatible water terrain");cell->LandType=land;
    bool edge=false;
    for(short y=0;y<160 && !edge;++y) for(short x=0;x<160 && !edge;++x) {
        CellStruct p{x,y},q{static_cast<short>(x+1),static_cast<short>(y+1)};
        if(!map.IsWithinUsableArea(p,false) || map.IsWithinUsableArea(q,false)) continue;
        check(!spawn(type,owner,p),"reject foundation extending outside map");edge=true;
    }
    check(edge,"map edge covered");
}
void verify() {
    int count,power;DWORD holderId;std::ifstream saved("spawn-building.ids");saved>>count>>power>>holderId;
    if(plants()!=count+1) std::ofstream("spawn-building.log",std::ios::app)<<"plant count "<<plants()<<" expected "<<count+1<<'\n';
    check(saved.good() && plants()==count+1,"pending INI result executes exactly once");
    Objects objects;for(auto u:TechnoClass::Array) if(alive(u)) objects.emplace(u->UniqueID,u);
    int checked=0;DWORD id;short x,y;int owner;
    while(saved>>id>>x>>y>>owner) {
        auto u=findLive(objects,id);
        check(u && u->WhatAmI()==AbstractType::Building && u->IsOnMap && !u->InLimbo &&
              u->GetMapCoords()==CellStruct{x,y} && u->Owner->ArrayIndex==owner,"building identity position and owner preserved");
        ++checked;
    }
    auto holder=findLive(objects,holderId);
    check(checked>=5 && holder && holder->Owner->PowerOutput>power,"spawned buildings supply power");
    dispatch();check(plants()==count+1,"no duplicate creation");
}
}
DEFINE_HOOK(0x55B4EB,SF_TestSpawnBuilding,5) {
    if(GetFileAttributesA("Starfall.test")==INVALID_FILE_ATTRIBUTES) return 0;
    const bool loading=GetFileAttributesA("reload.flag")!=INVALID_FILE_ATTRIBUTES;
    const int frame=Unsorted::CurrentFrame;
    if(!loading && frame==20) {
        Research::ensureReady();config();
        auto house=HouseClass::FindByPlayerAt(HouseClass::PlayerAtA);
        auto foe=HouseClass::FindByPlayerAt(HouseClass::PlayerAtB);
        auto type=BuildingTypeClass::Find("GAPOWR");
        check(house && foe && type,"houses and building type");
        const int power=house->PowerOutput;
        auto holder=fixture(UnitTypeClass::Find("MTNK"),house,{46,72});
        auto victim=fixture(UnitTypeClass::Find("MTNK"),foe,{85,80});
        check(skills.contains("randomreward") && !skills.contains("invalidrandom"),"random result validation");
        grantToUnit(holder,PassiveGrant{{"randomreward"},-1});
        int credits=house->Available_Money();
        capture("onkill",holder,victim,holder,victim,0);dispatch();
        check(house->Available_Money()==credits+21,"random result weights and repeated draws");
        capture("onkill",holder,victim,holder,victim,0);dispatch();
        check(house->Available_Money()==credits+21,"random result shared cooldown");
        {
            Skill random{};random.id="randomspawn";random.trigger="onkill";
            ScriptFields fields{{"action","spawnunit"},{"action.unittypes","E1,MTNK"},{"action.weights","0,1"},{"action.randomcount","3"},{"action.target","victimlocation"}};
            Event e=eventSnapshot("onkill",holder,victim,holder,victim,0);e.hasImpact=true;e.impact=coords({94,75});e.victim=0;
            Objects live;live.emplace(holder->UniqueID,holder);
            int before=UnitClass::Array.Count;
            check(readAction(random,random.id,&fields) && apply(random.actions.front(),e,holder,live,0) && UnitClass::Array.Count==before+3,"random spawn draws with replacement");
        }
        grantToUnit(holder,PassiveGrant{{"buildondeploy","luabuild"},-1});
        check(spawn(type,house,{50,65}),"exact placement");
        auto exact=buildingAt({50,65});
        check(exact && exact->Type==type && exact->GetMapCoords()==CellStruct{50,65} && exact->Owner==house,"exact anchor and owner");
        int count=BuildingClass::Array.Count,objects=TechnoClass::Array.Count;
        check(!spawn(type,house,{50,65}) && !spawn(type,house,{50,65},1) &&
              count==BuildingClass::Array.Count && objects==TechnoClass::Array.Count,"blocked radius fails without leaked objects");
        check(spawn(type,house,{50,65},3) && buildingAt({50,63}) && buildingAt({50,65})==exact,
              "nearest deterministic fallback preserves blocker");
        auto infantry=fixture(InfantryTypeClass::Find("E1"),house,{61,66});
        check(!buildingAt({60,65}) && !spawn(type,house,{60,65}),"partial foundation obstruction rejected");
        check(spawn(type,house,{60,65},3) && buildingAt({60,64}) && infantry->IsAlive && infantry->GetMapCoords()==CellStruct{61,66},
              "fallback preserves occupying infantry");
        terrain(type,house);
        bool defeated=foe->Defeated;foe->Defeated=true;
        check(!spawn(type,foe,{70,75}),"defeated owner rejected");foe->Defeated=defeated;
        capture("onfire",holder,victim,holder,victim,0);dispatch();
        check(buildingAt({80,65}) && buildingAt({80,63}) && buildingAt({80,65})->Owner==foe &&
              buildingAt({80,63})->Owner==foe,"Lua direct and section actions with victim owner");
        Event event=eventSnapshot("onkill",holder,victim,holder,victim,0);
        event.hasImpact=true;event.impact=coords({90,65});event.victim=0;
        Objects live;live.emplace(holder->UniqueID,holder);
        Skill result{};result.id="location";result.trigger="onkill";
        ScriptFields fields{{"action","spawnbuilding"},{"action.buildingtype","gapowr"},{"action.target","victimlocation"},{"action.owner","victim"}};
        check(readAction(result,result.id,&fields) && apply(result.actions.front(),event,holder,live,0) &&
              buildingAt({90,65}) && buildingAt({90,65})->Owner==foe,"destroyed victim location and snapshotted owner");
        std::ofstream ids("spawn-building.ids");ids<<plants()<<' '<<power<<' '<<holder->UniqueID<<'\n';
        for(auto b:BuildingClass::Array) if(b && b->IsOnMap && !b->InLimbo) {
            auto p=b->GetMapCoords();ids<<b->UniqueID<<' '<<p.X<<' '<<p.Y<<' '<<b->Owner->ArrayIndex<<'\n';
        }
        ids.close();
        capture("ondeploy",holder,nullptr,holder,holder,0);
        const char* file=GetModuleHandleA("CnCNet-Spawner.dll")?"SpawnBuilding.sav":"Saved Games\\SpawnBuilding.sav";
        check(ScenarioClass::SaveGame(file,L"SpawnBuilding"),"saved pending building result");
    }
    if(frame==40) {verify();TerminateProcess(GetCurrentProcess(),0);}
    return 0;
}
