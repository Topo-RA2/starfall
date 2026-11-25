#include "../../../src/Starfall.cpp"
namespace {
void check(bool ok,const char* name) {
    std::ofstream("mount.log",std::ios::app)<<(ok?"PASS ":"FAIL ")<<name<<"\n";
    if(!ok) TerminateProcess(GetCurrentProcess(),2);
}
FootClass* make(const char* id,HouseClass* owner,CellStruct cell) {
    auto type=TechnoTypeClass::Find(id);
    auto result=spawnUnit(type,owner,MapClass::Instance.GetCellAt(cell)->GetCoordsWithBridge());
    check(result!=nullptr,"placed fixture");
    return result;
}
BuildingClass* buildingAt(const char* type,HouseClass* owner,CellStruct at) {
    auto cell=MapClass::Instance.GetCellAt(at);
    check(BuildingSpawn::create(BuildingTypeClass::Find(type),owner,cell->GetCoords(),0),"place mount building");
    return cell->GetBuilding();
}
void checkDismantling(HouseClass* owner,const PassiveAction& installer) {
    auto parent=make("MTNK",owner,{48,70});
    auto& links=UnitMount::links;
    auto initial=links.size();
    grantToUnit(parent,PassiveGrant{{"upgradeleft"},-1});dispatch();
    check(links.size()==initial,"missing component stops INI upgrade");
    check(Mount::create(parent,installer),"install original left gun");
    auto old=links.rbegin()->second.child;
    grantToUnit(old,PassiveGrant{{"localshield","paymentondeath"},-1},true,parent);tickStates();
    auto credits=owner->Available_Money();
    removeHeldPassive(parent,"upgradeleft");
    grantToUnit(parent,PassiveGrant{{"upgradeleft"},-1});dispatch();dispatch();
    check(links.size()==initial+1 && links.rbegin()->second.slot=="left" &&
        links.rbegin()->second.child->GetTechnoType()==InfantryTypeClass::Find("E1") &&
        owner->Available_Money()==credits,"immune gun silently replaced in same slot");

    auto loose=installer;loose.mountSlot.clear();
    check(Mount::create(parent,loose),"install free vehicle");
    loose.unitType=InfantryTypeClass::Find("E1");
    check(Mount::create(parent,loose),"install first free infantry");
    auto older=links.rbegin()->first;
    check(Mount::create(parent,loose),"install second free infantry");
    auto newer=links.rbegin()->first;
    grantToUnit(parent,PassiveGrant{{"checkdismantling"},-1});dispatch();
    check(links.size()==initial+2 && !links.contains(older) && !links.contains(newer) &&
        owner->Available_Money()==credits+1,"Lua filters counts and failed sequence");
    grantToUnit(parent,PassiveGrant{{"recyclenewest"},-1});dispatch();
    check(links.size()==initial+1 && links.rbegin()->second.slot=="left","default removes latest matching attachment");
    check(Mount::create(parent,loose),"install additional gun for full removal");
    grantToUnit(parent,PassiveGrant{{"recycleall"},-1});dispatch();
    check(links.size()==initial,"Count All clears only the selected parent");
    check(consumeTarget(parent),"retire teardown fixture");Mount::tick();
    check(links.size()==initial,"teardown isolates other parents");
}
void installBuildingMount(HouseClass* owner,HouseClass* enemy,const PassiveAction& installer) {
    auto plant=buildingAt("GAPOWR",owner,{50,65});
    grantToUnit(plant,PassiveGrant{{"install"},-1});tickGains();dispatch();Mount::tick();
    auto& link=UnitMount::links.rbegin()->second;
    check(link.parent==plant && link.slot=="roof" && !Mount::create(plant,installer),"building installs via Self and reserves roof slot");
    auto roof=plant->GetCenterCoords()+CoordStruct{-48,-32,128};
    check(link.child->Location==roof && plant->GetCell()->GetBuilding()==plant,"building center and foundation preserved");
    plant->PrimaryFacing.SetCurrent(DirStruct(DirType::East));Mount::tick();
    check(link.child->Location==roof,"body anchor stays fixed while building aims");
    auto barracks=BuildingTypeClass::Find("NAHAND");
    check(canTransformUnit(plant,barracks),"occupied plant eligible for conversion");
    auto speed=barracks->SpeedType;barracks->SpeedType=SpeedType::Float;
    bool converted=transformUnit(plant,barracks);
    barracks->SpeedType=speed;
    check(!converted && link.parent==plant && link.child->IsOnMap && plant->GetCell()->GetBuilding()==plant,
        "failed building placement retains roof and foundation");
    check(transformUnit(plant,barracks),"convert building with occupied roof");
    Mount::tick();
    check(link.parent!=plant && link.parent->UniqueID==link.parentId && link.child->Location==Mount::position(link),"roof follows replacement building");

    link.parent->SetOwningHouse(enemy,false);Mount::tick();
    check(link.child->Owner==enemy,"captured building transfers child");
    link.parent->SetOwningHouse(owner,false);Mount::tick();

    auto tower=buildingAt("GTGCAN",owner,{60,65});
    auto gun=installer;gun.mountSlot.clear();gun.mountAnchor="turret";gun.mountOffset={96,0,112};
    check(Mount::create(tower,gun),"attach to gun turret");
    auto& turret=UnitMount::links.rbegin()->second;
    tower->PrimaryFacing.SetCurrent(DirStruct(DirType::North));Mount::tick();
    const auto north=turret.child->Location;
    tower->PrimaryFacing.SetCurrent(DirStruct(DirType::East));Mount::tick();
    check(turret.child->Location==north+CoordStruct{96,96,0},"turret rotates relative to fixed foundation");
    auto target=make("MTNK",enemy,{62,65});Mount::order(tower,target);Mount::tick();
    check(turret.child->Target==target,"building attack order reaches mounted gun");
    auto gunID=turret.child->UniqueID;
    int lethal=tower->Health*10;
    tower->ReceiveDamage(&lethal,0,RulesClass::Instance->C4Warhead,target,true,false,enemy);
    Mount::tick();
    check(!UnitMount::links.contains(gunID),"destroying tower removes mounted gun");
}
}
DEFINE_HOOK(0x55B4EB,SF_TestUnitMount,5) {
    if(GetFileAttributesA("Starfall.test")==INVALID_FILE_ATTRIBUTES) return 0;
    static DWORD soldBuilding=0,soldChild=0;
    int frame=Unsorted::CurrentFrame;
    if(frame==35 && GetFileAttributesA("reload.flag")==INVALID_FILE_ATTRIBUTES) {
        auto owner=HouseClass::CurrentPlayer;
        auto parent=make("MTNK",owner,{55,62});
        auto dummy=make("MTNK",HouseClass::FindByPlayerAt(HouseClass::PlayerAtB),{57,62});
        check(skills.contains("install"),"registered result");
        grantToUnit(parent,PassiveGrant{{"install"},-1});tickGains();dispatch();
        auto& links=UnitMount::links;
        check(links.size()==1,"installation via gained passive");
        auto a=skills.at("install").actions.front();
        check(Mount::create(parent,a) && !Mount::create(parent,a) && links.size()==2,"two slots and full rejection");
        auto child=links.begin()->second.child;
        auto& l=links.begin()->second;
        check(l.slot=="left" && l.parent==parent,"ordered slots");
        auto second=std::next(links.begin())->second.child;
        check(consumeTarget(second),"child removed independently");
        auto named=a;named.mountSlot.clear();named.mountNode="right";
        check(Mount::create(parent,named),"vacated named slot reused");
        check(!Mount::create(parent,a),"named install reserves automatic slot");
        Mount::order(parent,dummy);Mount::tick();
        check(child->Target==dummy,"child follows explicit parent attack");
        Mount::order(parent,nullptr);
        check(!l.ordered && Mount::acquire(child),"child finds enemy independently");
        ForceMove::Spec launch;
        launch.height=384;launch.duration=2;
        check(ForceMove::start(parent,launch,parent->Location),"parent launch");
        ForceMove::motions.at(parent->UniqueID).elapsed=1;
        Mount::tick();
        check(child->IsInAir() && l.trackerIndex>=0 && child->Location==Mount::position(l),"child follows parent into air targeting grid");
        ForceMove::forget(parent->UniqueID);Mount::tick();
        check(!child->IsInAir() && l.trackerIndex==-1,"landing releases air registration");
        auto controller=make("YURI",dummy->Owner,{62,62});
        check(controller->CaptureManager && !controller->CaptureManager->CanCapture(child) &&
            !controller->CaptureManager->CaptureUnit(child),"mounted unit rejects mind control");
        parent->PrimaryFacing.SetCurrent(DirStruct(DirType::East));Mount::tick();
        check(child->Location==Mount::position(l) && !child->Destination,"turn tracks physical coordinates");
        check(!grantToUnit(child,PassiveGrant{{"localshield"},-1}) && grantToUnit(child,PassiveGrant{{"localshield"},-1},true,parent),"parent-only grants");
        grantToUnit(parent,PassiveGrant{{"parentbuff","skipme","shield"},-1});tickStates();
        check(std::abs(child->FirepowerMultiplier-1.5)<0.01 && std::abs(child->ArmorMultiplier-1.0)<0.01 &&
            multiShotSettings(child,dummy).count==1,"filtered state inheritance");
        auto damage=[&](){int amount=20;child->ReceiveDamage(&amount,0,RulesClass::Instance->C4Warhead,dummy,false,false,dummy->Owner);};
        auto hp=child->Health;
        removeHeldPassive(parent,"shield");tickStates();damage();check(child->Health==hp,"child immunity survives parent loss");
        grantToUnit(parent,PassiveGrant{{"shield"},-1});removeHeldPassive(child,"localshield");tickStates();damage();
        check(child->Health==hp,"parent immunity survives exclusion");
        removeHeldPassive(parent,"shield");tickStates();damage();
        check(child->Health<hp,"damage resumes when both immunities end");
        check(!HouseClass::CurrentPlayer->Defeated,"live owner");
        checkDismantling(owner,a);
        auto plane=make("ORCA",owner,{60,62});
        for(int i=0;i<4;++i) check(Mount::create(plane,a),"aircraft slot");
        check(!Mount::create(plane,a),"four slots and full rejection");
        auto free=a;free.mountSlot.clear();free.mountOffset={92,-98,36};free.transportFire=false;
        check(Mount::create(parent,free),"free mount on full parent");
        auto silent=links.rbegin()->second.child;
        auto carrier=make("BFRT",owner,{56,62});check(carrier->GetTechnoType()->OpenTopped,"open carrier");
        parent->Limbo();carrier->EnterAsPassenger(parent);parent->Transporter=carrier;
        Mount::order(carrier,dummy);
        child->RearmTimer.Stop();int firedBefore=dummy->Health;
        std::set<DWORD> old;for(auto b:BulletClass::Array) old.insert(b->UniqueID);
        Mount::tick();
        bool fired=false;for(auto b:BulletClass::Array) if(!old.contains(b->UniqueID) && b->Owner==child) fired=true;
        check(fired || dummy->Health<firedBefore,"transport fires");
        check(child->RearmTimer.GetTimeLeft()>0,"transport respects weapon rearm");
        for(auto b:BulletClass::Array) check(old.contains(b->UniqueID) || b->Owner!=silent,"disabled mount holds fire");
        check(carrier->Passengers.NumPassengers==1 && child->InLimbo,"one seat and hidden child");
        carrier->Passengers.RemoveFirstPassenger();parent->Transporter=nullptr;
        check(parent->Unlimbo(MapClass::Instance.GetCellAt(CellStruct{55,62})->GetCoords(),DirType::East),"parent unload");
        Mount::tick();check(!child->InLimbo && child->Location==Mount::position(l),"unload restores mount");
        parent->SetOwningHouse(dummy->Owner,false);Mount::tick();
        check(child->Owner==dummy->Owner,"child follows capture");
        parent->SetOwningHouse(owner,false);Mount::tick();
        parent->Limbo();carrier->EnterAsPassenger(parent);parent->Transporter=carrier;
        Mount::order(carrier,dummy);Mount::tick();
        installBuildingMount(owner,dummy->Owner,a);
        check(ScenarioClass::SaveGame("Saved Games\\UnitMount.sav",L"UnitMount"),"mounted save");
    }
    if(frame==42) {
        Mount::bindLoaded();auto& links=UnitMount::links;
        check(links.size()==8 && links.begin()->second.parent && links.begin()->second.child,"restored slots and references");
        auto& l=links.begin()->second;
        auto p=static_cast<FootClass*>(l.parent);
        auto carrier=p->Transporter;
        check(carrier && carrier->Passengers.NumPassengers==1 && l.child->InLimbo &&
            l.carrierId==carrier->UniqueID && l.ordered && l.orderId==l.ordered->UniqueID,"transport and attack order survived save");
        auto newest=std::find_if(links.rbegin(),links.rend(),[&](const auto& pair){return pair.second.parent==p;});
        check(newest!=links.rend() && newest->second.slot.empty(),"free gun is newest after save");
        auto newestId=newest->first;
        auto remove=skills.at("recyclenewest").actions.front();
        Event event{};event.holder=carrier->UniqueID;
        Objects objects{{p->UniqueID,p},{carrier->UniqueID,carrier}};
        check(apply(remove,event,carrier,objects,0,p->UniqueID) && !links.contains(newestId) &&
            carrier->Passengers.NumPassengers==1 && p->Transporter==carrier,"recycle last installed gun inside saved transport");
        carrier->Passengers.RemoveFirstPassenger();p->Transporter=nullptr;
        check(p->Unlimbo(MapClass::Instance.GetCellAt(CellStruct{55,62})->GetCoords(),DirType::East),"unload saved passenger");
        Mount::tick();
        check(!l.child->InLimbo && l.child->Location==Mount::position(l),"saved attachment emerges");
        check(consumeTarget(p),"parent silently removed");Mount::tick();
        check(links.size()==5,"parent removal");
        auto& roof=links.rbegin()->second;
        check(roof.parent && roof.parent->WhatAmI()==AbstractType::Building &&
            roof.slot=="roof" && roof.child->Location==Mount::position(roof),"saved building roof restored");
        soldBuilding=roof.parentId;soldChild=roof.child->UniqueID;
        roof.parent->Sell(-1);
    }
    if(frame>42) {
        if(UnitMount::links.size()==4 && !UnitMount::links.contains(soldChild)) {
            bool retired=true;
            for(auto unit:TechnoClass::Array)
                if(alive(unit) && (unit->UniqueID==soldBuilding || unit->UniqueID==soldChild)) retired=false;
            check(retired,"sold building clears child");
            TerminateProcess(GetCurrentProcess(),0);
        }
        if(frame>=400) check(false,"building sale completed in time");
    }
    return 0;
}
