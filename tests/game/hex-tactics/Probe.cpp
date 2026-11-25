#include "../../../src/Starfall.cpp"
#include <ParasiteClass.h>
#include <TacticalClass.h>

namespace {
DWORD droneId=0,victimId=0,nearId=0,farId=0,infId=0,friendId=0,tanyaId=0,attackerId=0,roboId=0;
DWORD saleBaselineId=0,saleResearchId=0;
int saleMoney=0;
DWORD isolatedTanyaId=0,isolatedEnemyId=0;
int blastFrame=0,nearHp=0,farHp=0,infHp=0,friendHp=0;
void check(bool ok,const char* label) {
    std::ofstream("hex-tactics.log",std::ios::app)<<(ok?"PASS ":"FAIL ")<<label<<'\n';
    if(!ok) TerminateProcess(GetCurrentProcess(),2);
}
TechnoClass* find(DWORD id) {for(auto u:TechnoClass::Array) if(u->UniqueID==id) return u;return nullptr;}
TechnoClass* place(const char* name,HouseClass* owner,CellStruct cell={0,0}) {
    auto type=TechnoTypeClass::Find(name);check(type!=nullptr,"fixture type");
    auto u=static_cast<TechnoClass*>(type->CreateObject(owner));check(u!=nullptr,"fixture object");
    if(cell==CellStruct{0,0}) {
        bool found=false;
        for(short y=45;y<95 && !found;y+=5) for(short x=45;x<95 && !found;x+=5) {
            CellStruct c{x,y};if(BuildingSpawn::available(BuildingTypeClass::Find("NALASR"),owner,c)) {cell=c;found=true;}
        }
        check(found,"fixture space");
    }
    check(u->Unlimbo(MapClass::Instance.GetCellAt(cell)->GetCoords(),DirType::North),"fixture placed");
    u->QueueMission(Mission::Sleep,false);u->NextMission();return u;
}
void settle() {expireWarheadGrants();discoverAuras=true;refreshAuras();dispatch();tickGains();tickStates();dispatch();tickStates();}
void unlock(HouseClass* owner,const char* id,Research::Rarity rarity) {
    int index=-1;for(int i=0;i<int(Research::options.size());++i) if(Research::options[i].id==id) index=i;
    check(index>=0 && Research::available(index) && Research::options[index].rarity==rarity,"new project valid");
    Research::unlocked[owner->ArrayIndex].insert(id);Research::clearOfferCache();settle();
}
void hit(TechnoClass* victim,TechnoClass* killer,int amount=100000) {
    victim->ReceiveDamage(&amount,0,WarheadTypeClass::Find("AP"),killer,true,false,killer?killer->Owner:nullptr);settle();
}
void recovery(HouseClass* owner,HouseClass* foe) {
    auto killer=place("MTNK",foe);
    for(const auto id:{"MTNK","HARV","CMIN","HTK","AMCV","ROBO"}) {
        auto u=place(id,owner);const int expected=static_cast<int>(u->GetTechnoType()->Cost*.3);
        const int money=owner->Available_Money();hit(u,killer);
        check(owner->Available_Money()==money+expected,"ground vehicle death refunds exactly base cost thirty percent");
    }
    for(const auto id:{"ZEP","SHAD","DRED"}) {
        auto type=TechnoTypeClass::Find(id);auto u=static_cast<TechnoClass*>(type->CreateObject(owner));
        check(!groundVehicle(u) && !ownsPassive(u,"sf_hx_battlefieldrecovery_p"),"ships and airborne vehicle types excluded");u->UnInit();
    }
    auto changed=place("MTNK",owner);int money=owner->Available_Money();
    check(transformUnit(changed,UnitTypeClass::Find("HTNK")),"transform fixture succeeds");settle();
    check(owner->Available_Money()==money,"transformation gives no death refund");
    auto carrier=static_cast<UnitClass*>(place("BFRT",owner));
    auto cargo=static_cast<FootClass*>(UnitTypeClass::Find("MTNK")->CreateObject(owner));
    carrier->EnterAsPassenger(cargo);cargo->Transporter=carrier;
    Event event{};money=owner->Available_Money();check(consumePassenger(carrier,event),"consume fixture succeeds");settle();
    check(owner->Available_Money()==money,"consumed vehicle gives no refund");
    auto removed=place("MTNK",owner);money=owner->Available_Money();removed->RegisterDestruction(nullptr);removed->UnInit();settle();
    check(owner->Available_Money()==money,"nonfatal removal registration gives no refund");
    auto depot=place("GADEPT",owner,{55,80});const auto depotCell=depot->GetMapCoords();
    auto onDepot=[&]() {
        auto unit=static_cast<FootClass*>(place("MTNK",owner,{54,80}));
        unit->Mark(MarkType::Up);unit->SetLocation(MapClass::Instance.GetCellAt(depotCell)->GetCoords());
        unit->Mark(MarkType::Down);ForceMove::syncCellRegistration(unit);return unit;
    };
    auto baseline=onDepot();const auto baseId=baseline->UniqueID;
    Research::unlocked[owner->ArrayIndex].erase("sf_hx_battlefieldrecovery");Research::clearOfferCache();
    money=owner->Available_Money();baseline->Sell(-1);settle();const int refund=owner->Available_Money()-money;
    auto sold=onDepot();const auto soldId=sold->UniqueID;
    Research::unlocked[owner->ArrayIndex].insert("sf_hx_battlefieldrecovery");Research::clearOfferCache();
    money=owner->Available_Money();sold->Sell(-1);settle();
    check(refund>0 && owner->Available_Money()-money==refund,"native sale pays only ordinary sale refund");
    saleBaselineId=baseId;saleResearchId=soldId;saleMoney=owner->Available_Money();
}
void conviction(HouseClass* owner,HouseClass* foe) {
    auto boris=place("BORIS",owner,{70,70});auto ally=place("MTNK",owner,{71,70});auto enemy=place("MTNK",foe,{70,71});
    auto second=place("BORIS",owner,{71,71});settle();
    check(std::abs(boris->ArmorMultiplier-1.5)<1e-6 && std::abs(ally->FirepowerMultiplier-1.3)<1e-6 &&
        enemy->FirepowerMultiplier==1 && mindImmuneUnits.contains(ally->UniqueID),"Boris armor and nonstacking friendly aura");
    auto controller=place("MIND",foe);
    if(!controller->CaptureManager) controller->CaptureManager=GameCreate<CaptureManagerClass>(controller,3,false);
    check(!controller->CaptureManager->CanCapture(ally) && !controller->CaptureManager->CaptureUnit(ally) && ally->Owner==owner,
        "native mind control blocked by Boris aura");
    boris->Limbo();second->Limbo();settle();
    check(ally->FirepowerMultiplier==1 && !mindImmuneUnits.contains(ally->UniqueID) && controller->CaptureManager->CanCapture(ally),
        "leaving aura restores damage and mind control eligibility");
    boris->UnInit();second->UnInit();
}
void initDrone(HouseClass* owner,HouseClass* foe) {
    check(grantRules.contains(WarheadTypeClass::Find("SF_HX_ChainReactionWH")),"blast vehicle grant rule loaded");
    auto victim=static_cast<FootClass*>(place("HTNK",foe,{80,70}));victimId=victim->UniqueID;
    auto nearby=place("HTNK",foe,{81,70});nearId=nearby->UniqueID;nearHp=nearby->Health;
    auto distant=place("HTNK",foe,{83,70});farId=distant->UniqueID;farHp=distant->Health;
    auto infantry=place("E2",foe,{80,71});infId=infantry->UniqueID;infHp=infantry->Health;
    auto friendly=place("MTNK",owner,{79,70});friendId=friendly->UniqueID;friendHp=friendly->Health;
    auto drone=static_cast<FootClass*>(place("DRON",owner,{80,69}));droneId=drone->UniqueID;
    check(drone->ParasiteImUsing!=nullptr,"native terror drone parasite exists");
    victim->Health=victim->EstimatedHealth=1;
    drone->ParasiteImUsing->TryInfect(victim);
    check(drone->ParasiteImUsing->Victim==victim && victim->ParasiteEatingMe==drone,"native terror drone infects enemy vehicle");
    drone->ParasiteImUsing->DamageDeliveryTimer.Start(1);
}
void dodge(HouseClass* owner,HouseClass* foe) {
    auto tanya=place("TANY",owner,{65,80});tanyaId=tanya->UniqueID;
    auto enemy=place("HTNK",foe,{68,80});enemy->Deactivate();enemy->IronCurtain(10000,foe,false);attackerId=enemy->UniqueID;settle();
    // Reproduce each native chance draw from a copy, then compare intercepted HP.
    int dodged=0,hitCount=0;
    for(int i=0;i<100;++i) {
        const int before=tanya->Health;auto rng=ScenarioClass::Instance->Random;
        const bool expected=rng.RandomRanged(1,100)<=70;
        int damage=20;tanya->ReceiveDamage(&damage,0,WarheadTypeClass::Find("SA"),enemy,false,false,foe);
        const bool blocked=tanya->Health==before;
        if(blocked!=expected) check(false,"native seventy percent dodge uses one independent chance per hit");
        if(blocked) ++dodged;else ++hitCount;
        tanya->Health=tanya->EstimatedHealth=tanya->GetTechnoType()->Strength;
    }
    check(true,"native seventy percent dodge uses one independent chance per hit");
    check(dodged>0 && hitCount>0 && scriptTimers.queue.size()==1,"dodge has no cooldown and queues one safe teleport");
}
void ids(bool load) {
    if(load) std::ifstream("hex-tactics.ids")>>tanyaId>>attackerId>>roboId;
    else std::ofstream("hex-tactics.ids")<<tanyaId<<' '<<attackerId<<' '<<roboId;
}
}
#include "BattleEffects.inc"
#include "FireSupport.inc"
DEFINE_HOOK(0x55B4EB,SF_TestHexTactics,5) {
    if(GetFileAttributesA("Starfall.test")==INVALID_FILE_ATTRIBUTES) return 0;
    const int f=Unsorted::CurrentFrame;const bool load=GetFileAttributesA("reload.flag")!=INVALID_FILE_ATTRIBUTES;
    auto owner=HouseClass::FindByPlayerAt(HouseClass::PlayerAtA),foe=HouseClass::FindByPlayerAt(HouseClass::PlayerAtB);
    checkFireSupport(f,load,owner,foe);
    checkBattleEffects(f,load,owner,foe);
    if(!load && f==30) {
        Research::ensureReady();
        unlock(owner,"sf_hx_chainreaction",Research::Rarity::Rare);unlock(owner,"sf_hx_battlefieldrecovery",Research::Rarity::Rare);
        unlock(owner,"sf_hx_maglevacceleration",Research::Rarity::Common);unlock(owner,"sf_hx_redconviction",Research::Rarity::Epic);
        unlock(owner,"sf_hx_agilereflexes",Research::Rarity::Epic);
        auto robo=place("ROBO",owner);roboId=robo->UniqueID;settle();
        check(std::abs(static_cast<FootClass*>(robo)->SpeedMultiplier-1.3)<1e-6 &&
            std::abs(unitFactor(roboId,fireRateBase,fireRateState)-1.3)<1e-6,"robot tank speed and fire rate thirty percent");
        recovery(owner,foe);conviction(owner,foe);initDrone(owner,foe);
    }
    if(!load && f>30 && f<100) {
        settle();
        if(f==40) check(!alive(find(saleBaselineId)) && !alive(find(saleResearchId)) && owner->Available_Money()==saleMoney,
            "native sold vehicles disappear without delayed death refund");
        if(!blastFrame && (!find(victimId) || !alive(find(victimId)))) {
            blastFrame=f;
            std::ofstream("hex-tactics.log",std::ios::app)<<"blast "<<f<<" near "<<find(nearId)->Health<<'/'<<nearHp<<" infantry "<<find(infId)->Health<<'/'<<infHp
                <<" far "<<find(farId)->Health<<'/'<<farHp<<" friend "<<find(friendId)->Health<<'/'<<friendHp<<" drone "<<alive(find(droneId))
                <<" pending "<<pending.size()<<" counts "<<counts[droneId].size()<<" emp "<<empLocked(find(nearId))<<'\n';
            check(find(nearId)->Health==nearHp-90 && find(infId)->Health==infHp-90 && find(farId)->Health==farHp && find(friendId)->Health==friendHp,
                "parasite kill detonates ninety damage within one point five cells without friendly fire");
            check(empLocked(find(nearId)) && !empLocked(find(infId)),"blast EMP affects vehicles only");
            auto wh=WarheadTypeClass::Find("SF_HX_ChainReactionWH");auto elite=WeaponTypeClass::Find("BlimpBombE");
            check(wh->AnimList.Count && elite && elite->Warhead && wh->AnimList[0]==elite->Warhead->AnimList[0],"blast uses elite Kirov animation");
        }
        if(blastFrame && f==blastFrame+14) check(empLocked(find(nearId)),"EMP lasts through frame fourteen");
        if(blastFrame && f==blastFrame+15) check(!empLocked(find(nearId)),"EMP ends at fifteen frames");
    }
    if(!load && f==100) {check(blastFrame>0,"native parasite kill completed");dodge(owner,foe);}
    if(!load && f==101) {
        tickScriptTimers();settle();
        const auto key=LuaTimers::StateKey{tanyaId,"sf_hx_agilereflexes_p","nextTeleport"};
        check(scriptTimers.state.contains(key) && scriptTimers.state.at(key).integer==146,"successful teleport starts separate forty five frame cooldown");
        auto tanya=static_cast<FootClass*>(find(tanyaId));auto at=tanya->GetMapCoords();
        const auto target=find(attackerId)->GetMapCoords();int dx=at.X-target.X,dy=at.Y-target.Y;
        check(!tanya->IsImmobilized && !tanya->WarpingOut && tanya->LastMapCoords==at && dx*dx+dy*dy>0 && dx*dx+dy*dy<=4,
            "instant teleport updates map registration without native chrono freeze");
        ids(false);check(ScenarioClass::SaveGame("Saved Games\\HexTactics.sav",L"Hex tactics"),"saved with teleport cooldown");
    }
    if(load) ids(true);
    if(f==120) {
        auto tanya=find(tanyaId),enemy=find(attackerId);check(tanya && enemy,"saved actors");
        const auto before=scriptTimers.queue.size();int blocks=0;
        for(int n=0;n<20;++n) if(adjustPendingDamage("onbeforetakedamage",tanya,enemy,enemy,tanya,10)==0) ++blocks;
        check(blocks>0 && scriptTimers.queue.size()==before,"teleport cooldown never disables dodge");
        check(std::abs(static_cast<FootClass*>(find(roboId))->SpeedMultiplier-1.3)<1e-6,"research stats persist after load");
    }
    if(f==146) {
        auto tanya=find(tanyaId),enemy=find(attackerId);auto before=scriptTimers.queue.size();
        for(int n=0;n<50 && scriptTimers.queue.size()==before;++n) adjustPendingDamage("onbeforetakedamage",tanya,enemy,enemy,tanya,10);
        check(scriptTimers.queue.size()==before+1,"teleport available at exact cooldown boundary");
    }
    if(f==147) {
        tickScriptTimers();
        const auto key=LuaTimers::StateKey{tanyaId,"sf_hx_agilereflexes_p","nextTeleport"};
        check(scriptTimers.state.at(key).integer==192,"second teleport succeeds immediately after cooldown");
    }
    if(f==240) {
        auto tanya=find(tanyaId),enemy=find(attackerId);
        const auto at=tanya->GetMapCoords(),target=enemy->GetMapCoords();
        int dx=at.X-target.X,dy=at.Y-target.Y;
        check(alive(tanya) && tanya->Owner==owner && !tanya->IsImmobilized && dx*dx+dy*dy>0 && dx*dx+dy*dy<=4,
            "teleport arrives on empty ground near attacking vehicle with original identity");
    }
    if(f==250) {
        auto tanya=place("TANY",owner,{95,55}),enemy=place("E2",foe,{98,55});
        isolatedTanyaId=tanya->UniqueID;isolatedEnemyId=enemy->UniqueID;settle();
        check(adjustPendingDamage("onbeforetakedamage",tanya,find(roboId),find(roboId),tanya,10)==10,"friendly damage does not trigger enemy dodge");
        int blocked=0;
        for(int n=0;n<20;++n) if(adjustPendingDamage("onbeforetakedamage",tanya,enemy,enemy,tanya,10)==0) ++blocked;
        check(blocked>0,"no enemy vehicle still permits dodge");
    }
    if(f==251) {
        tickScriptTimers();
        const auto key=LuaTimers::StateKey{isolatedTanyaId,"sf_hx_agilereflexes_p","nextTeleport"};
        check(!scriptTimers.state.contains(key) && find(isolatedTanyaId)->GetMapCoords()==CellStruct{95,55},"no vehicle leaves position and teleport cooldown unchanged");
        find(isolatedEnemyId)->UnInit();
        auto enemy=place("MTNK",foe,{98,55});isolatedEnemyId=enemy->UniqueID;
        for(short dy=-2;dy<=2;++dy) for(short dx=-2;dx<=2;++dx)
            if(dx*dx+dy*dy>0 && dx*dx+dy*dy<=4) place("NALASR",foe,{short(98+dx),short(55+dy)});
        auto tanya=find(isolatedTanyaId);settle();
        int blocked=0;for(int n=0;n<20;++n) if(adjustPendingDamage("onbeforetakedamage",tanya,enemy,enemy,tanya,10)==0) ++blocked;
        check(blocked>0,"blocked landing still permits dodge");
    }
    if(f==252) {
        tickScriptTimers();
        const auto key=LuaTimers::StateKey{isolatedTanyaId,"sf_hx_agilereflexes_p","nextTeleport"};
        check(!scriptTimers.state.contains(key) && find(isolatedTanyaId)->GetMapCoords()==CellStruct{95,55},"blocked landing leaves position and teleport cooldown unchanged");
        check(true,"completed five tactics projects");TerminateProcess(GetCurrentProcess(),0);
    }
    return 0;
}
