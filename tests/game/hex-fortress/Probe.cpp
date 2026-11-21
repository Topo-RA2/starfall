#include "../../../src/Starfall.cpp"

namespace {
constexpr auto newTypeId="SF_HX_SuperFortressUnit";
constexpr auto projectId="sf_hx_superfortress";
DWORD fortressId=0,infantryId=0,futureId=0,enemyId=0,tankId=0,targetId=0;
int basePrice=0,enemyPrice=0,savedFrame=0;
CellStruct origin{};
void check(bool ok,const char* label) {
    std::ofstream("hex-fortress.log",std::ios::app)<<(ok?"PASS ":"FAIL ")<<label<<'\n';
    if(!ok) TerminateProcess(GetCurrentProcess(),2);
}
TechnoClass* find(DWORD id) {
    for(auto u:TechnoClass::Array) if(u->UniqueID==id) return u;
    return nullptr;
}
TechnoClass* place(const char* id,HouseClass* house,int x,int y) {
    auto type=TechnoTypeClass::Find(id);
    auto unit=type?static_cast<TechnoClass*>(type->CreateObject(house)):nullptr;
    CellStruct pos{short(origin.X+x),short(origin.Y+y)};
    auto cell=MapClass::Instance.TryGetCellAt(pos);
    check(unit && cell && unit->Unlimbo(cell->GetCoordsWithBridge(),DirType::North),"placed fixture");
    unit->QueueMission(Mission::Sleep,false);
    return unit;
}
FootClass* addPassenger(UnitClass* carrier,const char* id) {
    auto p=static_cast<FootClass*>(TechnoTypeClass::Find(id)->CreateObject(carrier->Owner));
    check(p!=nullptr,"created passenger");
    carrier->EnterAsPassenger(p);p->Transporter=carrier;
    if(!p->InOpenToppedTransport) carrier->EnteredOpenTopped(p);
    p->SetLocation(carrier->GetCoords());
    return p;
}
void settle() {tickGains();tickStates();dispatch();}
void stats(TechnoClass* u) {
    check(u && u->GetTechnoType()==UnitTypeClass::Find(newTypeId) &&
        std::abs(u->ArmorMultiplier-2.0)<0.0001 &&
        std::abs(static_cast<FootClass*>(u)->SpeedMultiplier-0.70)<0.0001 &&
        VisualScale::percent(u->UniqueID)==200,"fortress armor scale and speed");
}
void prices(HouseClass* owner,HouseClass* foe) {
    check(UnitTypeClass::Find("BFRT")->GetActualCost(owner)==basePrice*3 &&
        UnitTypeClass::Find(newTypeId)->GetActualCost(owner)==basePrice*3 &&
        UnitTypeClass::Find("BFRT")->GetActualCost(foe)==enemyPrice,"triple production price only for researcher");
}
void writeIds() {
    std::ofstream("hex-fortress.ids")<<fortressId<<' '<<infantryId<<' '<<futureId<<' '<<enemyId<<' '
        <<tankId<<' '<<targetId<<' '<<basePrice<<' '<<enemyPrice<<' '<<savedFrame;
}
void readIds() {
    std::ifstream("hex-fortress.ids")>>fortressId>>infantryId>>futureId>>enemyId
        >>tankId>>targetId>>basePrice>>enemyPrice>>savedFrame;
}
}

DEFINE_HOOK(0x55B4EB,SF_TestHexFortress,5) {
    if(GetFileAttributesA("Starfall.test")==INVALID_FILE_ATTRIBUTES) return 0;
    const int frame=Unsorted::CurrentFrame;
    const bool loading=GetFileAttributesA("reload.flag")!=INVALID_FILE_ATTRIBUTES;
    auto owner=HouseClass::FindByPlayerAt(HouseClass::PlayerAtA);
    auto foe=HouseClass::FindByPlayerAt(HouseClass::PlayerAtB);
    if(loading) {
        readIds();if(frame<savedFrame+10) return 0;
        auto u=static_cast<UnitClass*>(find(fortressId));
        settle();stats(u);stats(find(futureId));prices(owner,foe);
        auto infantry=find(infantryId),tank=find(tankId);
        check(u && u->Passengers.NumPassengers==8 && infantry && tank &&
            infantry->Transporter==u && tank->Transporter==u && tank->InOpenToppedTransport &&
            u->Veterancy.Veterancy==1.0f && Research::owns(owner->ArrayIndex,projectId),
            "loaded fortress research identity rank and mixed passengers");
        auto unloaded=u->RemoveFirstPassenger();
        check(unloaded && unloaded->UniqueID==tankId,"last tank is first to unload");
        u->ExitedOpenTopped(unloaded);unloaded->Transporter=nullptr;
        auto pos=u->GetMapCoords();pos.Y+=3;
        check(unloaded->Unlimbo(MapClass::Instance.GetCellAt(pos)->GetCoordsWithBridge(),DirType::North) &&
            u->Passengers.NumPassengers==7 && !unloaded->InOpenToppedTransport,"loaded tank unloads normally");
        check(unloaded->MouseOverObject(u)==Action::Enter,"unloading reopens one slot");
        origin=u->GetMapCoords();
        auto loadedCarrier=static_cast<UnitClass*>(place("BFRT",owner,8,2));
        for(int i=0;i<5;++i) addPassenger(loadedCarrier,"BRUTE");
        check(loadedCarrier->Passengers.GetTotalSize()>8 && transformUnit(loadedCarrier,UnitTypeClass::Find(newTypeId)) &&
            loadedCarrier->Passengers.NumPassengers==5,"count based conversion remains available after load");
        TerminateProcess(GetCurrentProcess(),0);return 0;
    }
    if(frame==30) {
        Research::ensureReady();
        bool clear=false;
        for(short y=35;!clear && y<85;++y) for(short x=30;!clear && x<85;++x) {
            bool empty=true;
            for(short dy=0;empty && dy<16;++dy) for(short dx=0;empty && dx<22;++dx) {
                CellStruct p{short(x+dx),short(y+dy)};auto cell=MapClass::Instance.TryGetCellAt(p);
                empty=cell && MapClass::Instance.IsWithinUsableArea(p,false) && !cell->FirstObject &&
                    !cell->SlopeIndex && cell->LandType!=LandType::Water;
            }
            if(empty) {origin={x,y};clear=true;}
        }
        check(clear,"clear fixture area");
        auto u=static_cast<UnitClass*>(place("BFRT",owner,6,6));fortressId=u->UniqueID;
        auto enemy=place("BFRT",foe,19,12);enemyId=enemy->UniqueID;
        auto tank=static_cast<UnitClass*>(place("MTNK",owner,8,6));tankId=tank->UniqueID;
        u->Veterancy.Veterancy=1.0f;
        infantryId=addPassenger(u,"GGI")->UniqueID;
        for(int i=0;i<4;++i) addPassenger(u,"BRUTE");
        check(u->Passengers.NumPassengers==5 && u->Passengers.GetTotalSize()>8 &&
            canTransformUnit(u,UnitTypeClass::Find(newTypeId)),"full large infantry cargo can transform by count");
        mapPassengersBySize[lower(newTypeId)]=true;
        check(!canTransformUnit(u,UnitTypeClass::Find(newTypeId)),"size based capacity still rejects oversized cargo");
        mapPassengersBySize[lower(newTypeId)]=false;
        check(u->Type->Passengers==5 && tank->MouseOverObject(u)!=Action::Enter &&
            VisualScale::percent(u->UniqueID)==100,"ordinary fortress unchanged before research");
        basePrice=u->Type->GetActualCost(owner);enemyPrice=u->Type->GetActualCost(foe);
        int index=-1;
        for(int i=0;i<static_cast<int>(Research::options.size());++i)
            if(Research::options[i].id==projectId) index=i;
        check(index>=0 && Research::options[index].rarity==Research::Rarity::Epic && Research::available(index),
            "epic fortress project available");
        Research::award(owner->ArrayIndex,100);
        // Choose this card deterministically, then exercise the real purchase path.
        Research::savedOffers[owner->ArrayIndex]={projectId};Research::clearOfferCache();
        check(Research::purchase(owner->ArrayIndex,index,owner->Type->ID),"purchased fortress research");
    }
    if(frame==35) {
        auto u=static_cast<UnitClass*>(find(fortressId));auto tank=static_cast<UnitClass*>(find(tankId));
        settle();stats(u);prices(owner,foe);
        auto factory=static_cast<FactoryClass*>(YRMemory::AllocateChecked(sizeof(FactoryClass)));
        reinterpret_cast<void(__thiscall*)(FactoryClass*)>(0x4C98B0)(factory);factory->Owner=owner;
        check(factory->DemandProduction(UnitTypeClass::Find("BFRT"),owner,false) && factory->Balance==basePrice*3,
            "factory charges upgraded price");
        check(factory->AbandonProduction(),"cancel test production");delete factory;
        check(u->Passengers.NumPassengers==5 && find(infantryId)->Transporter==u &&
            u->Veterancy.Veterancy==1.0f && u->Owner==owner,"conversion preserves passenger identity owner and rank");
        check(find(enemyId)->GetTechnoType()==UnitTypeClass::Find("BFRT"),"other house fortress remains unchanged");
        check(u->Type->Passengers==8 && u->Type->SizeLimit>=tank->GetTechnoType()->Size && u->Type->OpenTopped &&
            u->Type->TechLevel==-1 && tank->MouseOverObject(u)==Action::Enter,"tank entry and eight-slot rules");
        auto future=place("BFRT",owner,14,5);futureId=future->UniqueID;
        for(int i=0;i<2;++i) addPassenger(u,i%2?"HTNK":"MTNK");
        check(u->Passengers.NumPassengers==7 && u->Passengers.GetTotalSize()>8 &&
            tank->MouseOverObject(u)==Action::Enter,"tank occupies one slot regardless of Size");
        tank->ObjectClickedAction(Action::Enter,u,false);
    }
    if(frame>=40 && !savedFrame) {
        auto u=static_cast<UnitClass*>(find(fortressId));auto tank=static_cast<UnitClass*>(find(tankId));
        if(frame==239) std::ofstream("hex-fortress.log",std::ios::app)
            <<"boarding: count="<<u->Passengers.NumPassengers<<" mission="<<int(tank->CurrentMission)
            <<" destination="<<(tank->Destination==u)<<" tank="<<tank->Location.X<<','<<tank->Location.Y
            <<" carrier="<<u->Location.X<<','<<u->Location.Y<<" radio="<<int(tank->SendCommand(RadioCommand::QueryCanEnter,u))<<'\n';
        if(frame>=240) check(false,"tank entry completes within deadline");
        if(u->Passengers.NumPassengers!=8) return 0;
        check(tank->Transporter==u && tank->InLimbo && tank->InOpenToppedTransport,"native tank boarding completes");
        settle();stats(find(futureId));
        check(find(futureId)->GetTechnoType()==UnitTypeClass::Find(newTypeId),"future production transforms automatically");
        auto extra=static_cast<UnitClass*>(place("MTNK",owner,11,9));
        check(extra->MouseOverObject(u)!=Action::Enter,"ninth passenger rejected");
        check(static_cast<UnitClass*>(find(futureId))->MouseOverObject(u)!=Action::Enter,"fortress nesting rejected");
        auto target=place("HTNK",foe,6,10);targetId=target->UniqueID;
        tank->SetLocation(u->GetCoords());tank->RearmTimer.Stop();tank->SetTarget(target);
        auto facing=tank->GetTargetDirection(target);tank->PrimaryFacing.SetCurrent(facing);tank->SecondaryFacing.SetCurrent(facing);
        auto weapon=tank->GetWeapon(0)->WeaponType;auto projectile=weapon->Projectile;
        weapon->Projectile=BulletTypeClass::Find("Cannon"); // Keep the round observable for the assertion.
        auto shot=tank->Fire(target,0);
        if(!shot) for(auto b:BulletClass::Array) if(b->Owner==tank) {shot=b;break;}
        weapon->Projectile=projectile;
        if(!shot) std::ofstream("hex-fortress.log",std::ios::app)<<"fire error="<<int(tank->GetFireError(target,0,true))<<'\n';
        check(shot && shot->Owner==tank && shot->Target==target,"tank fires from open-topped fortress");
        shot->UnInit();tank->SetTarget(nullptr);
        savedFrame=frame;writeIds();
        IStream* stream=nullptr;
        check(SUCCEEDED(CreateStreamOnHGlobal(nullptr,TRUE,&stream)) && saveState(stream),"capacity mode save stream");
        LARGE_INTEGER end{};end.QuadPart=-4;stream->Seek(end,STREAM_SEEK_END,nullptr);
        DWORD invalidBool=2;stream->Write(&invalidBool,sizeof(invalidBool),nullptr);stream->Seek({},STREAM_SEEK_SET,nullptr);
        check(!loadState(stream) && Research::owns(owner->ArrayIndex,projectId) &&
            !mapPassengersBySize.at(lower(newTypeId)),"invalid capacity mode rejected without changing live state");
        stream->Release();
        check(ScenarioClass::SaveGame("Saved Games\\HexFortress.sav",L"Super Fortress"),"saved loaded fortress");
    }
    if(savedFrame && frame>savedFrame+1) TerminateProcess(GetCurrentProcess(),0);
    return 0;
}
