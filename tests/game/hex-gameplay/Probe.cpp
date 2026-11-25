#include "../../../src/Starfall.cpp"
#include <ScriptClass.h>

namespace {
TechnoClass *sniper=nullptr,*victim=nullptr;
CellStruct offset{};
CellStruct fixtureCell(CellStruct p) {return {short(p.X+offset.X),short(p.Y+offset.Y)};}
void check(bool ok,const char* label) {
    std::ofstream("hex-gameplay.log",std::ios::app)<<(ok?"PASS ":"FAIL ")<<label<<"\n";
    if(!ok) TerminateProcess(GetCurrentProcess(),2);
}
TechnoClass* place(const char* id,HouseClass* owner,CellStruct pos) {
    pos=fixtureCell(pos);
    auto type=TechnoTypeClass::Find(id);
    auto unit=type?static_cast<TechnoClass*>(type->CreateObject(owner)):nullptr;
    auto cell=MapClass::Instance.TryGetCellAt(pos);
    const bool placed=unit && cell && unit->Unlimbo(cell->GetCoordsWithBridge(),DirType::North);
    if(!placed) std::ofstream("hex-gameplay.log",std::ios::app)<<id<<" at "<<pos.X<<","<<pos.Y<<"\n";
    check(placed,"placed fixture");
    unit->QueueMission(Mission::Sleep,false);
    return unit;
}
bool has(TechnoClass* unit,const char* id) {
    vector<string> held;extraSkills(unit,&held);return includes(held,id);
}
void settle() {expireWarheadGrants();refreshAuras();dispatch();tickGains();tickStates();dispatch();}
void unlock(HouseClass* owner,const char* id,Research::Rarity rarity) {
    int index=-1;
    for(int i=0;i<static_cast<int>(Research::options.size());++i)
        if(Research::options[i].id==id) index=i;
    check(index>=0 && Research::options[index].rarity==rarity && Research::available(index),"research available with expected rarity");
    Research::unlocked[owner->ArrayIndex].insert(id);Research::clearOfferCache();
    discoverAuras=true;
}
bool boosted(TechnoClass* unit) {
    return std::abs(unit->ArmorMultiplier-1.2)<0.0001 &&
        std::abs(unitFactor(unit->UniqueID,fireRateBase,fireRateState)-1.2)<0.0001;
}
void shrinkHit() {
    auto weapon=WeaponTypeClass::Find("SF_HX_ShrinkRayWeapon");
    auto bullet=weapon->Projectile->CreateBullet(victim,sniper,weapon->Damage,weapon->Warhead,weapon->Speed,false);
    check(bullet!=nullptr,"created shrink round");
    bullet->SetWeaponType(weapon);
    auto at=victim->GetCoords();bullet->SetLocation(at);bullet->Detonate(at);bullet->UnInit();
    settle();
}
#include "EnemyResearch.inc"
void newResearch(HouseClass* owner,HouseClass* foe) {
    auto tank=place("HTNK",owner,{50,73});
    unlock(owner,"sf_hx_shrinkengine",Research::Rarity::Epic);settle();
    check(VisualScale::percent(tank->UniqueID)==60 &&
        std::abs(static_cast<FootClass*>(tank)->SpeedMultiplier-1.30)<0.0001 &&
        std::abs(unitFactor(tank->UniqueID,fireRateBase,fireRateState)-1.40)<0.0001,"shrink engine vehicle modifiers");

    unlock(owner,"sf_hx_huntingcontest",Research::Rarity::Common);
    for(auto id:{"ORCA","BEAG"}) {
        auto plane=place(id,owner,{52,73});auto enemy=place("HTNK",foe,{53,73});settle();
        const int money=owner->Available_Money(),bounty=enemy->GetTechnoType()->Cost/2;
        int fatal=100000;enemy->ReceiveDamage(&fatal,0,RulesClass::Instance->C4Warhead,plane,true,false,owner);settle();
        check(owner->Available_Money()==money+bounty,"aircraft kill pays half base cost");
        plane->Limbo();plane->UnInit();
    }

    auto disk=place("DISK",owner,{50,68});
    unlock(owner,"sf_hx_paralysisray",Research::Rarity::Rare);settle();
    for(auto id:{"HTNK","ORCA"}) {
        auto target=place(id,foe,{51,68});
        for(int i=0;i<250 && !empLocked(target);++i) {
            int damage=1;target->Health=target->GetTechnoType()->Strength;
            target->ReceiveDamage(&damage,0,RulesClass::Instance->C4Warhead,disk,true,false,owner);settle();
        }
        check(empLocked(target) && warheadGrants.at(target->UniqueID).skills.at("sf_hx_shortcircuit_emp").remaining(Unsorted::CurrentFrame)==105,
            "disk can paralyze vehicle and aircraft for 105 frames");
        target->Limbo();target->UnInit();
    }

    unlock(owner,"sf_hx_brainwash",Research::Rarity::Epic);
    for(auto id:{"YURI","YURIPR"}) {
        auto yuri=place(id,owner,{50,70});settle();
        const bool prime=std::string_view(id)=="YURIPR";
        auto weapon=yuri->GetWeapon(0)->WeaponType;
        check(weapon==WeaponTypeClass::Find(prime?"SF_HX_PermControlX":"SF_HX_PermControl") &&
            weapon->ROF==(prime?900:600),"brainwash replaces mind control weapon and triples interval");
        auto first=place("HTNK",foe,{51,70});
        auto second=place(prime?"NALASR":"MTNK",foe,{53,70});
        for(auto target:{first,second}) {
            yuri->SetTarget(target);yuri->RearmTimer.Stop();
            yuri->PrimaryFacing.SetCurrent(yuri->GetTargetDirection(target));
            auto shot=yuri->Fire(target,0);check(shot!=nullptr,"native mind control shot fired");
            auto coords=target->GetCoords();shot->SetLocation(coords);shot->Detonate(coords);shot->UnInit();
            check(target->Owner==owner,"mind control transfers ownership");
        }
        yuri->Limbo();yuri->UnInit();settle();
        check(first->Owner==owner && second->Owner==owner,"both captures survive retargeting and controller removal");
        for(auto target:{first,second}) {target->Limbo();target->UnInit();}
    }
}
void clusterShot(TechnoClass* shooter,TechnoClass* primary,TechnoClass* extra) {
    const int slot=shooter->SelectWeapon(primary);
    shooter->SetTarget(primary);shooter->RearmTimer.Stop();
    auto facing=shooter->GetTargetDirection(primary);
    shooter->PrimaryFacing.SetCurrent(facing);shooter->SecondaryFacing.SetCurrent(facing);
    std::set<DWORD> before;
    for(auto b:BulletClass::Array) before.insert(b->UniqueID);
    shooter->Fire(primary,slot);
    std::set<DWORD> hit;std::vector<BulletClass*> shots;
    for(auto b:BulletClass::Array) if(b->Owner==shooter && !before.contains(b->UniqueID)) {
        shots.push_back(b);if(b->Target) hit.insert(b->Target->UniqueID);
    }
    check(shots.size()==2 && hit==std::set<DWORD>{primary->UniqueID,extra->UniqueID},"cluster fires at two distinct enemies");
    for(auto b:shots) b->UnInit();
    shooter->SetTarget(nullptr);shooter->Deactivate();
}
}

DEFINE_HOOK(0x55B4EB,SF_TestHexGameplay,5) {
    if(GetFileAttributesA("Starfall.test")==INVALID_FILE_ATTRIBUTES) return 0;
    const int frame=Unsorted::CurrentFrame;
    if(frame==30) {
        Research::ensureReady();
        auto owner=HouseClass::CurrentPlayer,foe=HouseClass::FindByPlayerAt(HouseClass::PlayerAtB);
        auto neutral=HouseClass::FindNeutral();
        check(owner && foe && neutral,"test houses");
        bool clear=false;
        for(short y=35;!clear && y<85;++y) for(short x=30;!clear && x<85;++x) {
            bool empty=true;
            for(short dy=0;empty && dy<18;++dy) for(short dx=0;empty && dx<26;++dx) {
                CellStruct p{short(x+dx),short(y+dy)};auto cell=MapClass::Instance.TryGetCellAt(p);
                empty=cell && MapClass::Instance.IsWithinUsableArea(p,false) && !cell->FirstObject && !cell->SlopeIndex && cell->LandType!=LandType::Water;
            }
            if(empty) {offset={short(x-40),short(y-60)};clear=true;}
        }
        check(clear,"clear fixture area");
        const auto ownerAllies=owner->Allies,foeAllies=foe->Allies;
        owner->Allies.Add(foe);foe->Allies.Add(owner);
        auto coil=place("TESLA",owner,{55,65});
        auto own=place("MTNK",owner,{58,65}),ally=place("MTNK",foe,{57,64});
        auto distant=place("MTNK",owner,{62,65}),outsider=place("MTNK",neutral,{57,66});
        auto infantry=place("E1",owner,{56,66});
        unlock(owner,"sf_hx_energysurge",Research::Rarity::Rare);settle();
        check(boosted(own) && boosted(ally) && !boosted(distant) && !boosted(outsider) && !boosted(infantry),"energy aura range allies and vehicle filter");
        auto second=place("TESLA",owner,{55,62});discoverAuras=true;settle();
        check(boosted(own) && auraGrants.at(own->UniqueID).at("sf_hx_energysurge_buff")==2,"energy overlap does not stack and new coils inherit");
        check(own->Limbo() && own->Unlimbo(MapClass::Instance.GetCellAt(fixtureCell({63,66}))->GetCoordsWithBridge(),DirType::North),"moved outside aura");
        settle();check(!boosted(own) && own->ArmorMultiplier==1.0,"energy aura withdraws on exit");
        for(auto source:{coil,second}) {
            int damage=INT_MAX/4;source->ReceiveDamage(&damage,0,RulesClass::Instance->C4Warhead,nullptr,true,false,nullptr);
        }
        settle();check(!has(ally,"sf_hx_energysurge_buff") && ally->ArmorMultiplier==1.0,"energy aura withdraws on source destruction");
        owner->Allies=ownerAllies;foe->Allies=foeAllies;
        sniper=place("VIRUS",owner,{45,73});victim=place("MTNK",foe,{47,73});
        auto other=place("MTNK",foe,{46,74});auto foot=place("E1",foe,{46,72});
        auto primary=sniper->GetWeapon(0)->WeaponType;
        unlock(owner,"sf_hx_shrinkray",Research::Rarity::Rare);settle();
        auto ray=sniper->GetWeapon(sniper->SelectWeapon(victim))->WeaponType;
        check(ray && !_stricmp(ray->ID,"SF_HX_ShrinkRayWeapon") && sniper->SelectWeapon(foot)==0 &&
            ray->Damage==primary->Damage && ray->ROF==primary->ROF && ray->Range==primary->Range &&
            ray->Warhead->InfDeath==primary->Warhead->InfDeath,
            "shrink weapon selected for tanks with infantry combat retained");
        sniper->RearmTimer.Stop();sniper->SetTarget(victim);
        auto facing=sniper->GetTargetDirection(victim);sniper->PrimaryFacing.SetCurrent(facing);
        auto bullet=sniper->Fire(victim,sniper->SelectWeapon(victim));
        check(bullet && bullet->WH==ray->Warhead,"native sniper fires shrink weapon");
        auto at=victim->GetCoords();bullet->SetLocation(at);bullet->Detonate(at);bullet->UnInit();
        sniper->SetTarget(nullptr);sniper->Deactivate();settle();
        check(has(victim,"sf_hx_shrinkray_debuff") && VisualScale::percent(victim->UniqueID)==80 &&
            std::abs(victim->FirepowerMultiplier-0.6)<0.0001,"shrink hit reduces size and outgoing damage");
        unlock(owner,"sf_hx_clustermissiles",Research::Rarity::Common);
        auto gi=place("GGI",owner,{44,73});auto patriot=place("NASAM",owner,{40,73});settle();
        check(multiShotSettings(gi,victim).count==1 && multiShotSettings(patriot,victim).count==1 &&
            multiShotSettings(sniper,victim).count==0,"cluster granted only to patriot and guardian GI");
        // Remove the infantry candidate so the pair of nearby tanks is unambiguous.
        foot->Limbo();clusterShot(gi,victim,other);
        unlock(owner,"sf_hx_prismconversion",Research::Rarity::Epic);
        auto conscript=place("E2",owner,{42,70});
        unlock(owner,"sf_hx_levelup",Research::Rarity::Common);
        conscript->Veterancy.Veterancy=0;previousState.erase(conscript->UniqueID);observeTransitions();dispatch();
        const auto initialMax=UnitHealth::read(conscript).maximum;
        const auto firstMax=initialMax+(initialMax*50+50)/100;
        const auto secondMax=firstMax+(firstMax*50+50)/100;
        conscript->Veterancy.Veterancy=1;observeTransitions();dispatch();
        check(UnitHealth::read(conscript).maximum==firstMax && std::abs(conscript->FirepowerMultiplier-1.2)<0.0001 &&
            VisualScale::percent(conscript->UniqueID)==150,"level up veteran grants health damage and size");
        conscript->Veterancy.Veterancy=2;observeTransitions();dispatch();
        check(UnitHealth::read(conscript).maximum==secondMax && std::abs(conscript->FirepowerMultiplier-1.4)<0.0001 &&
            VisualScale::percent(conscript->UniqueID)==200,"level up elite accumulates permanent bonuses");
        observeTransitions();dispatch();
        Research::unlocked[owner->ArrayIndex].erase("sf_hx_levelup");settle();
        check(UnitHealth::read(conscript).maximum==secondMax && std::abs(conscript->FirepowerMultiplier-1.4)<0.0001 &&
            VisualScale::percent(conscript->UniqueID)==200,"level up does not repeat and bonuses remain permanent");
        for(auto unit:TechnoClass::Array) unit->Deactivate();
    }
    if(frame==45) {
        shrinkHit();
        check(VisualScale::percent(victim->UniqueID)==80 && std::abs(victim->FirepowerMultiplier-0.6)<0.0001,
            "repeated shrink refreshes without stacking");
    }
    if(frame==60) {
        settle();check(has(victim,"sf_hx_shrinkray_debuff"),"shrink refresh extends original deadline");
    }
    if(frame==75) {
        settle();check(!has(victim,"sf_hx_shrinkray_debuff") && VisualScale::percent(victim->UniqueID)==100 &&
            victim->FirepowerMultiplier==1.0,"shrink expires and restores size and damage after two seconds");
        newResearch(HouseClass::CurrentPlayer,HouseClass::FindByPlayerAt(HouseClass::PlayerAtB));
        enemyResearch(HouseClass::FindByPlayerAt(HouseClass::PlayerAtB));
    }
    if(frame>75 && waypointTeamID) trackSplitTeam(frame);
    return 0;
}
