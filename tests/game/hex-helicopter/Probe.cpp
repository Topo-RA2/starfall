#include "../../../src/Starfall.cpp"

namespace {
constexpr auto projectId="sf_hx_fortresshelicopter";
constexpr auto variantId="SF_HX_FortressHeliUnit";
constexpr auto shotProbe="helicoptertestshots";
DWORD carrierId=0,rocketId=0,gunnerId=0,futureId=0,newGunnerId=0,enemyId=0,targetId=0;
int oldPrice=0;unsigned rocketShots=0,gunnerShots=0;
CellStruct origin{};
void check(bool ok,const char* label) {
    std::ofstream("hex-helicopter.log",std::ios::app)<<(ok?"PASS ":"FAIL ")<<label<<'\n';
    if(!ok) TerminateProcess(GetCurrentProcess(),2);
}
TechnoClass* find(DWORD id) {
    for(auto u:TechnoClass::Array) if(u->UniqueID==id) return u;
    return nullptr;
}
unsigned shotCount(DWORD id) {return triggerTotals[id][shotProbe];}
TechnoClass* place(const char* id,HouseClass* owner,int x,int y) {
    auto type=TechnoTypeClass::Find(id);
    auto u=type?static_cast<TechnoClass*>(type->CreateObject(owner)):nullptr;
    CellStruct pos{short(origin.X+x),short(origin.Y+y)};
    auto cell=MapClass::Instance.TryGetCellAt(pos);
    check(u && cell && u->Unlimbo(cell->GetCoordsWithBridge(),DirType::North),"placed fixture");
    u->QueueMission(Mission::Sleep,true);return u;
}
FootClass* passenger(UnitClass* carrier,const char* id) {
    auto p=static_cast<FootClass*>(TechnoTypeClass::Find(id)->CreateObject(carrier->Owner));
    check(p!=nullptr,"created passenger");
    carrier->EnterAsPassenger(p);p->Transporter=carrier;
    grantToUnit(p,PassiveGrant{{shotProbe},-1});return p;
}
void basicStats(UnitClass* u) {
    auto from=UnitTypeClass::Find("SHAD");auto to=u->Type;
    check(to==UnitTypeClass::Find(variantId) && to->OpenTopped && to->Passengers==from->Passengers &&
        to->SizeLimit==from->SizeLimit && to->Strength==from->Strength && to->Armor==from->Armor &&
        to->Speed==from->Speed && IsEqualGUID(to->Locomotor,from->Locomotor) &&
        to->Cost==from->Cost && to->GetActualCost(u->Owner)==oldPrice &&
        from->GetActualCost(u->Owner)==oldPrice && to->TechLevel==-1,"variant retains helicopter stats capacity weapons and price");
    check(u->GetWeapon(0)->WeaponType==from->GetWeapon(0,u->Veterancy.IsElite()).WeaponType,"original helicopter weapon retained");
}
void logFlight() {
    auto u=static_cast<UnitClass*>(find(carrierId));auto p=find(rocketId);
    std::ofstream("hex-helicopter.log",std::ios::app)<<"flight height="<<u->GetHeight()<<" mission="<<int(u->CurrentMission)
        <<" rocket shots="<<shotCount(rocketId)<<" gunner shots="<<shotCount(gunnerId)
        <<" passenger open="<<p->InOpenToppedTransport<<" target="<<(p->Target!=nullptr)
        <<" probe held="<<ownsPassive(p,shotProbe)<<'\n';
}
void writeIds() {
    std::ofstream("hex-helicopter.ids")<<carrierId<<' '<<rocketId<<' '<<gunnerId<<' '<<futureId<<' '
        <<newGunnerId<<' '<<enemyId<<' '<<targetId<<' '<<oldPrice<<' '<<shotCount(rocketId)<<' '<<shotCount(gunnerId);
}
void readIds() {
    std::ifstream("hex-helicopter.ids")>>carrierId>>rocketId>>gunnerId>>futureId
        >>newGunnerId>>enemyId>>targetId>>oldPrice>>rocketShots>>gunnerShots;
}
}

DEFINE_HOOK(0x55B4EB,SF_TestHexHelicopter,5) {
    if(GetFileAttributesA("Starfall.test")==INVALID_FILE_ATTRIBUTES) return 0;
    const int frame=Unsorted::CurrentFrame;
    const bool loading=GetFileAttributesA("reload.flag")!=INVALID_FILE_ATTRIBUTES;
    if(frame<20) return 0;
    Research::ensureReady();
    auto owner=HouseClass::FindByPlayerAt(HouseClass::PlayerAtA);
    auto foe=HouseClass::FindByPlayerAt(HouseClass::PlayerAtB);
    if(loading) {
        if(!carrierId) readIds();
        auto u=static_cast<UnitClass*>(find(carrierId));auto future=static_cast<UnitClass*>(find(futureId));
        if(frame==190) {
            basicStats(u);basicStats(future);
            auto rocket=find(rocketId),gunner=find(gunnerId),newGunner=find(newGunnerId);
            check(Research::owns(owner->ArrayIndex,projectId) && u->GetHeight()>128 && u->Passengers.NumPassengers==2 &&
                u->Veterancy.Veterancy==1.0f && rocket && gunner && rocket->Transporter==u && gunner->Transporter==u &&
                rocket->InOpenToppedTransport && gunner->InOpenToppedTransport && future->Passengers.NumPassengers==1 &&
                newGunner && newGunner->Transporter==future && newGunner->InOpenToppedTransport,
                "loaded airborne helicopter and passenger firing state retained");
            future->QueueMission(Mission::Unload,true);
        }
        if(frame==260) {
            logFlight();
            check(shotCount(rocketId)>rocketShots && shotCount(gunnerId)>gunnerShots,"passengers continue native airborne firing after load");
            auto p=find(newGunnerId);
            check(future->Passengers.NumPassengers==0 && p && !p->Transporter && !p->InOpenToppedTransport && p->IsOnMap && !p->InLimbo,
                "native helicopter unloading clears passenger firing state");
            Research::unlocked[owner->ArrayIndex].erase(projectId);Research::clearOfferCache();
            check(transformUnit(u,UnitTypeClass::Find("SHAD")) && u->Passengers.NumPassengers==2 &&
                !find(rocketId)->InOpenToppedTransport && !find(gunnerId)->InOpenToppedTransport,
                "conversion back to closed cabin clears passenger firing state");
            dispatch();rocketShots=shotCount(rocketId);gunnerShots=shotCount(gunnerId);
        }
        if(frame==300) {
            check(shotCount(rocketId)==rocketShots && shotCount(gunnerId)==gunnerShots,"closed cabin passengers stop firing");
            TerminateProcess(GetCurrentProcess(),0);
        }
        return 0;
    }
    if(frame==20) {
        bool clear=false;
        for(short y=35;!clear && y<85;++y) for(short x=30;!clear && x<85;++x) {
            bool empty=true;
            for(short dy=0;empty && dy<18;++dy) for(short dx=0;empty && dx<26;++dx) {
                CellStruct p{short(x+dx),short(y+dy)};auto cell=MapClass::Instance.TryGetCellAt(p);
                empty=cell && MapClass::Instance.IsWithinUsableArea(p,false) && !cell->FirstObject &&
                    !cell->SlopeIndex && cell->LandType!=LandType::Water;
            }
            if(empty) {origin={x,y};clear=true;}
        }
        check(clear,"clear fixture area");
        auto u=static_cast<UnitClass*>(place("SHAD",owner,5,6));carrierId=u->UniqueID;
        auto enemy=place("SHAD",foe,22,14);enemyId=enemy->UniqueID;
        rocketId=passenger(u,"GGI")->UniqueID;gunnerId=passenger(u,"E1")->UniqueID;
        u->Veterancy.Veterancy=1.0f;oldPrice=u->Type->GetActualCost(owner);
        check(!u->Type->OpenTopped && !find(rocketId)->InOpenToppedTransport && !find(gunnerId)->InOpenToppedTransport,
            "ordinary helicopter has closed cabin before research");
        CellStruct dest{short(origin.X+18),short(origin.Y+6)};
        u->SetDestination(MapClass::Instance.GetCellAt(dest),true);u->QueueMission(Mission::Move,true);
    }
    if(frame==65) {
        auto u=static_cast<UnitClass*>(find(carrierId));
        check(u->GetHeight()>128 && shotCount(rocketId)==0 && shotCount(gunnerId)==0,"research fixture is airborne with silent passengers");
        int index=-1;
        for(int i=0;i<static_cast<int>(Research::options.size());++i) if(Research::options[i].id==projectId) index=i;
        check(index>=0 && Research::options[index].rarity==Research::Rarity::Rare && Research::available(index),"rare helicopter project available");
        Research::award(owner->ArrayIndex,100);Research::savedOffers[owner->ArrayIndex]={projectId};Research::clearOfferCache();
        check(Research::purchase(owner->ArrayIndex,index,owner->Type->ID),"purchased helicopter research");
    }
    if(frame==70) {
        auto u=static_cast<UnitClass*>(find(carrierId));basicStats(u);
        logFlight();
        check(u->GetHeight()>128 && u->Veterancy.Veterancy==1.0f && u->Passengers.NumPassengers==2 &&
            find(rocketId)->Transporter==u && find(gunnerId)->Transporter==u &&
            find(rocketId)->InOpenToppedTransport && find(gunnerId)->InOpenToppedTransport,
            "airborne conversion activates existing passengers and preserves identity rank cargo");
        check(find(enemyId)->GetTechnoType()==UnitTypeClass::Find("SHAD") && !find(enemyId)->GetTechnoType()->OpenTopped,
            "other house helicopter remains closed");
        auto target=place("HTNK",foe,14,10);targetId=target->UniqueID;
        UnitHealth::change(target,30000);UnitHealth::heal(target,30000);
        u->ObjectClickedAction(Action::Attack,target,false);
        auto future=place("SHAD",owner,5,13);futureId=future->UniqueID;
        auto p=place("E1",owner,5,15);newGunnerId=p->UniqueID;
    }
    if(frame==75) {
        auto future=static_cast<UnitClass*>(find(futureId));basicStats(future);
        auto p=find(newGunnerId);
        check(p->MouseOverObject(future)==Action::Enter,"future helicopter accepts ordinary boarding");
        p->ObjectClickedAction(Action::Enter,future,false);
    }
    if(frame==180) {
        auto u=static_cast<UnitClass*>(find(carrierId));logFlight();
        check(u->GetHeight()>128 && shotCount(rocketId)>0 && shotCount(gunnerId)>0 && find(targetId)->Health<30000,
            "mixed passengers fire natively from airborne helicopter");
        auto p=find(newGunnerId);auto future=find(futureId);
        check(future->GetTechnoType()==UnitTypeClass::Find(variantId) && future->Passengers.NumPassengers==1 &&
            p->Transporter==future && p->InOpenToppedTransport,"future helicopter and newly boarded passenger gain open cabin");
        writeIds();
        check(ScenarioClass::SaveGame("Saved Games\\HexHelicopter.sav",L"Fortress Helicopter"),"saved airborne open cabin");
    }
    if(frame==185) TerminateProcess(GetCurrentProcess(),0);
    return 0;
}
