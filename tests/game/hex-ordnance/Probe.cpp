#include "../../../src/Starfall.cpp"
#include <BombListClass.h>

namespace {
struct Blast {int frame,damage;float radius;CoordStruct point;string wh;};
vector<Blast> blasts;
vector<DWORD> miners;
vector<string> minerWeapons;
DWORD planterId=0,firstBomb=0,secondBomb=0,missileId=0;
int baselineDamage=0;float baselineRadius=0;
bool loading=false;
void check(bool ok,const char* label) {
    std::ofstream("hex-ordnance.log",std::ios::app)<<(ok?"PASS ":"FAIL ")<<label<<'\n';
    if(!ok) TerminateProcess(GetCurrentProcess(),2);
}
TechnoClass* find(DWORD id) {for(auto u:TechnoClass::Array) if(u->UniqueID==id) return u;return nullptr;}
BombClass* bomb(DWORD id) {for(auto b:BombListClass::Instance.Bombs) if(b->Target && b->Target->UniqueID==id) return b;return nullptr;}
TechnoClass* place(const char* id,HouseClass* owner) {
    auto type=TechnoTypeClass::Find(id);check(type!=nullptr,"fixture type exists");
    auto u=static_cast<TechnoClass*>(type->CreateObject(owner));
    auto footprint=type->WhatAmI()==AbstractType::BuildingType?static_cast<BuildingTypeClass*>(type):BuildingTypeClass::Find("NALASR");
    bool placed=false;
    for(short y=40;y<96 && !placed;y+=5) for(short x=40;x<96 && !placed;x+=5) {
        CellStruct c{x,y};if(!BuildingSpawn::available(footprint,owner,c)) continue;
        placed=u->Unlimbo(MapClass::Instance.GetCellAt(c)->GetCoords(),DirType::North);
    }
    check(placed,"fixture placed");
    if(u->WhatAmI()==AbstractType::Building) {auto b=static_cast<BuildingClass*>(u);b->BeginMode(BStateType::Idle);b->Place(false);}
    u->QueueMission(Mission::Sleep,false);u->NextMission();return u;
}
void settle() {expireWarheadGrants();refreshAuras();refreshSpawnGrants();dispatch();tickGains();tickStates();dispatch();tickStates();}
void unlock(HouseClass* owner,const char* id,Research::Rarity rarity) {
    int index=-1;for(int i=0;i<int(Research::options.size());++i) if(Research::options[i].id==id) index=i;
    check(index>=0 && Research::available(index) && Research::options[index].rarity==rarity,"project parsed with correct rarity");
    Research::unlocked[owner->ArrayIndex].insert(id);Research::clearOfferCache();
}
void licenses(HouseClass* owner,HouseClass* foe) {
    place("NACNST",owner);place("GACNST",owner);
    auto clone=BuildingTypeClass::Find("NACLON"),cannon=BuildingTypeClass::Find("GTGCAN");
    auto native=owner->Type;
    owner->Type=HouseTypeClass::Find("Americans");
    check(owner->CanBuild(clone,false,false)!=CanBuildResult::Buildable && owner->CanBuild(cannon,false,false)!=CanBuildResult::Buildable,"licenses absent before research");
    unlock(owner,"sf_hx_cloningcenter",Research::Rarity::Epic);unlock(owner,"sf_hx_grandcannon",Research::Rarity::Rare);
    for(auto country:{"Americans","Russians"}) {
        owner->Type=HouseTypeClass::Find(country);
        check(owner->CanBuild(clone,false,false)==CanBuildResult::Buildable && owner->CanBuild(cannon,false,false)==CanBuildResult::Buildable,"allied and soviet native production unlocked");
    }
    owner->Type=HouseTypeClass::Find("YuriCountry");
    check(owner->CanBuild(cannon,false,false)==CanBuildResult::Buildable,"non French Yuri cannon unlocked");
    owner->Type=native;
    check(foe->CanBuild(cannon,false,false)!=CanBuildResult::Buildable,"other player receives no production license");
    place("NACLON",owner);
    check(owner->CanBuild(clone,false,false)!=CanBuildResult::Buildable,"cloning build limit retained");
    unlock(owner,"sf_hx_superdefense",Research::Rarity::Epic);
    check(owner->CanBuild(cannon,false,false)==CanBuildResult::Unbuildable && owner->CanBuild(BuildingTypeClass::Find("SF_HX_GiantGTGCAN"),false,false)==CanBuildResult::Buildable,"cannon license combines with giant defense");
}
void minerChecks(HouseClass* owner,HouseClass* foe) {
    unlock(owner,"sf_hx_armedminer",Research::Rarity::Epic);
    for(int i=0;i<18;++i) miners.push_back(place("HARV",owner)->UniqueID);
    settle();settle();
    std::set<string> choices;
    for(auto id:miners) {
        auto u=find(id);string weapon=u->GetWeapon(0)->WeaponType->ID;choices.insert(weapon);minerWeapons.push_back(weapon);
        check(includes({"TankBolt","120mmx","AKM"},weapon),"miner gets exactly one advanced ground weapon");
        check(static_cast<UnitClass*>(u)->Type->Harvester,"miner keeps harvesting behavior");
    }
    check(choices.size()>1,"miners roll independently");
    auto other=place("HARV",foe);settle();
    check(other->GetWeapon(0)->WeaponType==other->GetTechnoType()->GetWeapon(0,false).WeaponType,"other owner miner unchanged");
    auto u=find(miners.front());auto base=minerWeapons.front();u->Veterancy.Veterancy=2;settle();
    const auto elite=base=="TankBolt"?"TankBoltE":base=="120mmx"?"120mmxE":"AKME";
    check(string(u->GetWeapon(0)->WeaponType->ID)==elite,"miner elite uses same weapon family");
    u->Veterancy.Veterancy=0;settle();
}
DWORD plant(TechnoClass* source,HouseClass* enemy) {
    auto target=place("MTNK",enemy);target->Health=10000;
    source->SetTarget(target);source->RearmTimer.Stop();
    auto facing=source->GetTargetDirection(target);source->PrimaryFacing.SetCurrent(facing);source->SecondaryFacing.SetCurrent(facing);
    auto projectile=source->Fire(target,0);check(projectile!=nullptr,"native Ivan weapon fired");
    auto at=target->GetCoords();projectile->SetLocation(at);projectile->Detonate(at);projectile->UnInit();source->SetTarget(nullptr);
    auto b=target->AttachedBomb;check(b!=nullptr,"native bomb planted");b->DetonationFrame=10000;
    return target->UniqueID;
}
Blast explodeRocket(HouseClass* owner,bool elite) {
    auto rocket=static_cast<AircraftClass*>(place("V3ROCKET",owner));settle();
    auto loco=locomotion_cast<RocketLocomotionClass*>(rocket->Locomotor);check(loco!=nullptr,"native rocket locomotor");
    loco->SpawnerIsElite=elite;
    auto before=blasts.size();
    reinterpret_cast<void(__thiscall*)(RocketLocomotionClass*)>(0x663030)(loco);
    check(blasts.size()==before+1,"native rocket explosion captured");return blasts.back();
}
void rocketChecks(HouseClass* owner,HouseClass* foe) {
    auto normal=explodeRocket(foe,false),elite=explodeRocket(foe,true);
    unlock(owner,"sf_hx_bigrocket",Research::Rarity::Epic);settle();
    auto boosted=explodeRocket(owner,false),boostElite=explodeRocket(owner,true);
    check(boosted.damage==normal.damage*2 && boostElite.damage==elite.damage*2 && boosted.radius==normal.radius*2 && boostElite.radius==elite.radius*2,"normal and elite rocket damage and radius doubled");
    check(std::count_if(VisualScale::explosions.begin(),VisualScale::explosions.end(),[](const auto& e){return e.second==200;})>=2,"rocket explosion animations doubled");
    auto rocket=place("V3ROCKET",owner);missileId=rocket->UniqueID;settle();
    std::ofstream("hex-ordnance.log",std::ios::app)<<"rocket "<<rocket->Health<<'/'<<UnitHealth::read(rocket).maximum<<" base "<<rocket->GetTechnoType()->Strength<<" size "<<VisualScale::percent(missileId)<<'\n';
    check(UnitHealth::read(rocket).maximum==rocket->GetTechnoType()->Strength*2 && UnitHealth::read(rocket).current==rocket->GetTechnoType()->Strength*2 && VisualScale::percent(missileId)==200,"rocket health and visual scale doubled");
    int hit=rocket->GetTechnoType()->Strength+10;
    rocket->ReceiveDamage(&hit,0,WarheadTypeClass::Find("AP"),nullptr,true,false,foe);
    check(alive(rocket) && UnitHealth::read(rocket).current==rocket->GetTechnoType()->Strength-10,"rocket survives native damage beyond original health");
    UnitHealth::heal(rocket,rocket->GetTechnoType()->Strength*2);
    rocket->Limbo();
    check(spreadScopes.empty() && WarheadTypeClass::Find("V3WH")->CellSpread==normal.radius,"shared warhead radius restored");
}
void newProjects(HouseClass* owner,HouseClass* foe) {
    auto weapon=WeaponTypeClass::Find("SF_HX_InfantryNukeWeapon");
    check(weapon && weapon->Damage==120 && weapon->Warhead==WarheadTypeClass::Find("TerrorBombWH"),"mini nuke fixed 120 damage and terrorist warhead");
    for(auto name:{"sf_hx_goodbrothers","sf_hx_foundtreasure","sf_hx_infantrynuke","sf_hx_legacy","sf_hx_sniperlicense","sf_hx_moonlicense"}) {
        int index=-1;for(int i=0;i<int(Research::options.size());++i) if(Research::options[i].id==name) index=i;
        check(index>=0 && Research::available(index),"new project registered and usable");
    }
    auto index=-1;for(int i=0;i<int(Research::options.size());++i) if(Research::options[i].id=="sf_hx_foundtreasure") index=i;
    Research::points[owner->ArrayIndex]=100;
    Research::savedOffers[owner->ArrayIndex]={"sf_hx_foundtreasure"};Research::clearOfferCache();
    int credits=owner->Available_Money();
    check(Research::purchase(owner->ArrayIndex,index,owner->Type->ID) && owner->Available_Money()==credits+15000,"treasure pays 15000 immediately once");
    check(!Research::purchase(owner->ArrayIndex,index,owner->Type->ID),"treasure cannot pay twice");
    unlock(owner,"sf_hx_infantrynuke",Research::Rarity::Rare);
    auto flak=place("FLAKT",owner),target=place("MTNK",foe);
    auto before=passiveBullets.size();
    for(int i=0;i<100;++i) {capture("onfire",flak,target,flak,target,0);dispatch();}
    auto added=passiveBullets.size()-before;
    check(added>0 && added<100,"flak attacks probabilistically emit additional mini nuke projectiles");
    place("NAHAND",owner);
    unlock(owner,"sf_hx_sniperlicense",Research::Rarity::Common);unlock(owner,"sf_hx_moonlicense",Research::Rarity::Rare);
    auto country=owner->Type;owner->Type=HouseTypeClass::Find("Russians");
    check(owner->CanBuild(InfantryTypeClass::Find("SNIPE"),false,false)==CanBuildResult::Buildable && owner->CanBuild(InfantryTypeClass::Find("LUNR"),false,false)==CanBuildResult::Buildable,"soviet barracks supports both licensed units");
    owner->Type=country;place("GAPILE",owner);
    check(owner->CanBuild(InfantryTypeClass::Find("SNIPE"),false,false)==CanBuildResult::Buildable && owner->CanBuild(InfantryTypeClass::Find("LUNR"),false,false)==CanBuildResult::Buildable,"sniper and lunar infantry production licenses work");
    unlock(owner,"sf_hx_goodbrothers",Research::Rarity::Rare);unlock(owner,"sf_hx_legacy",Research::Rarity::Rare);
    auto holder=place("E2",owner);auto cell=holder->GetMapCoords();
    auto friendUnit=static_cast<TechnoClass*>(InfantryTypeClass::Find("E2")->CreateObject(owner));
    auto neighbor=CellStruct{static_cast<short>(cell.X+1),cell.Y};
    check(friendUnit->Unlimbo(MapClass::Instance.GetCellAt(neighbor)->GetCoords(),DirType::North),"nearby conscript fixture");
    settle();
    check(std::abs(holder->FirepowerMultiplier-1.05)<0.001 && std::abs(holder->ArmorMultiplier-1.05)<0.001,"one nearby conscript grants five percent");
    vector<TechnoClass*> crowd;
    for(int y=-2;y<=2 && crowd.size()<10;++y) for(int x=-2;x<=2 && crowd.size()<10;++x) {
        if((x==0 && y==0) || (x==1 && y==0)) continue;
        CellStruct c{static_cast<short>(cell.X+x),static_cast<short>(cell.Y+y)};
        if(!BuildingSpawn::available(BuildingTypeClass::Find("NALASR"),owner,c)) continue;
        auto u=static_cast<TechnoClass*>(InfantryTypeClass::Find("E2")->CreateObject(owner));
        if(u->Unlimbo(MapClass::Instance.GetCellAt(c)->GetCoords(),DirType::North)) crowd.push_back(u);else u->UnInit();
    }
    check(crowd.size()==10,"nearby conscript crowd fixture");nearbySamples.clear();settle();
    check(std::abs(holder->FirepowerMultiplier-1.5)<0.001 && std::abs(holder->ArmorMultiplier-1.5)<0.001,"conscript bonus caps at fifty percent");
    for(auto u:crowd) u->UnInit();settle();
    friendUnit->Health=20;friendUnit->EstimatedHealth=20;
    int damage=100000;holder->ReceiveDamage(&damage,0,WarheadTypeClass::Find("AP"),nullptr,true,false,foe);settle();
    check(friendUnit->Health==60 && unitFactor(friendUnit->UniqueID,fireRateBase,fireRateState)>1.39,"legacy heals and grants fire rate after death");
}
TechnoClass* neighbor(TechnoClass* center,const char* type,HouseClass* owner,short dx,short dy) {
    auto cell=center->GetMapCoords();
    cell.X+=dx;cell.Y+=dy;
    auto unit=static_cast<TechnoClass*>(TechnoTypeClass::Find(type)->CreateObject(owner));
    check(unit && unit->Unlimbo(MapClass::Instance.GetCellAt(cell)->GetCoords(),DirType::North),"nearby combat fixture");
    return unit;
}
void psychicAndGrowth(HouseClass* owner,HouseClass* foe) {
    const int house=owner->ArrayIndex;
    for(const char* id:{"sf_hx_scavenger","sf_hx_braindisruption","sf_hx_psychicreinforcement"}) {
        for(const char* country:{"Americans","Russians","YuriCountry"}) {
            Research::savedOffers[house]={id};
            Research::clearOfferCache();
            const auto& rows=Research::offers(house,country);
            const bool offered=std::any_of(rows.begin(),rows.end(),[&](int index){return Research::options[index].id==id;});
            check(offered==(std::string_view(country)=="YuriCountry"),"only Yuri can receive the Brute and Mastermind research offers");
        }
    }
    unlock(owner,"sf_hx_scavenger",Research::Rarity::Common);
    auto brute=place("BRUTE",owner);
    auto first=neighbor(brute,"BRUTE",owner,1,0),second=neighbor(brute,"BRUTE",owner,0,1);
    auto hostile=neighbor(brute,"BRUTE",foe,-1,0);
    int strength=UnitHealth::read(first).maximum;
    for(auto unit:{first,second,hostile}) unit->Health=unit->EstimatedHealth=20;
    int lethal=100000;
    brute->ReceiveDamage(&lethal,0,WarheadTypeClass::Find("AP"),nullptr,true,false,foe);
    settle();
    int recipients=0;
    for(auto unit:{first,second,hostile}) {
        auto hp=UnitHealth::read(unit);
        if(hp.maximum==strength) {
            check(hp.current==20 && VisualScale::percent(unit->UniqueID)==100,"unselected brute unchanged");
        } else {
            ++recipients;
            check(unit!=hostile && hp.maximum==strength+50 && hp.current==hp.maximum &&
                VisualScale::percent(unit->UniqueID)==110,"friendly brute gained health and size and healed to new maximum");
        }
    }
    check(recipients==1,"one scavenger per death");

    unlock(owner,"sf_hx_bigdog",Research::Rarity::Common);
    for(auto type:{"DOG","ADOG"}) {
        auto dog=place(type,owner);settle();
        const int base=UnitHealth::read(dog).maximum;
        check(std::abs(dog->ArmorMultiplier-1.2)<0.001,"dog gains armor before first kill");
        for(auto victim:{hostile,first,hostile}) {
            capture("onkill",dog,victim,dog,victim,0);dispatch();
        }
        check(UnitHealth::read(dog).maximum==base+60 && VisualScale::percent(dog->UniqueID)==140,
            "dog growth accumulates for enemy kills only");
    }

    unlock(owner,"sf_hx_braindisruption",Research::Rarity::Common);
    unlock(owner,"sf_hx_psychicreinforcement",Research::Rarity::Epic);
    auto car=place("MIND",owner);
    auto guard=neighbor(car,"E1",owner,1,0),enemy=neighbor(car,"E1",foe,0,1);
    auto secondCar=neighbor(car,"MIND",owner,2,0);
    discoverAuras=true;
    settle();
    check(std::abs(guard->ArmorMultiplier-1.5)<0.001 &&
        std::abs(unitFactor(guard->UniqueID,fireRateBase,fireRateState)-1.2)<0.001 &&
        enemy->ArmorMultiplier==1.0,"overlapping mastermind auras grant one friendly infantry bonus");
    for(int attempt=0;attempt<50 && !enemy->Berzerk;++attempt) {
        int damage=1;
        car->ReceiveDamage(&damage,0,WarheadTypeClass::Find("AP"),enemy,true,false,foe);
        settle();
    }
    check(enemy->Berzerk && warheadGrants.at(enemy->UniqueID).skills.at("sf_hx_braindisruption_berserk").remaining(Unsorted::CurrentFrame)==45,
        "damaged mastermind confuses its attacker for 45 frames");
    car->Limbo();secondCar->Limbo();settle();
    check(guard->ArmorMultiplier==1.0,"infantry loses aura when both sources leave");

    unlock(owner,"sf_hx_forkedcurrent",Research::Rarity::Epic);
    for(auto type:{"SHK","TESLA","SF_HX_GiantTESLA","TTNK"}) {
        auto shooter=place(type,owner);
        check(multiShotSettings(shooter,hostile).count==1,"Tesla unit gets one additional target");
        if(shooter->GetTechnoType()!=UnitTypeClass::Find("TTNK")) continue;
        auto primary=neighbor(shooter,"MTNK",foe,1,0);
        neighbor(shooter,"MTNK",foe,0,1);
        shooter->SetTarget(primary);
        shooter->RearmTimer.Stop();
        auto facing=shooter->GetTargetDirection(primary);
        shooter->PrimaryFacing.SetCurrent(facing);shooter->SecondaryFacing.SetCurrent(facing);
        const int beforeBolts=EBolt::Array.Count;
        shooter->Fire(primary,0);
        std::set<AbstractClass*> targets;
        for(auto shot:BulletClass::Array) if(shot->Owner==shooter) targets.insert(shot->Target);
        check(targets.size()==2 && targets.contains(primary),"native Tesla tank shot reaches two distinct targets");
        check(EBolt::Array.Count==beforeBolts+2 && EBolt::Array[beforeBolts]->Point2!=EBolt::Array[beforeBolts+1]->Point2,
            "Forked Current draws a separate arc for each hit");
    }
}
string tail() {
    string out;
    for(const auto& b:blasts) if(b.frame>60) out+=std::to_string(b.frame)+","+std::to_string(b.damage)+","+std::to_string(b.point.X)+","+std::to_string(b.point.Y)+","+b.wh+"\n";
    return out;
}
void ids(bool write) {
    if(write) {std::ofstream out("hex-ordnance.ids");out<<secondBomb<<' '<<missileId<<' '<<baselineDamage<<' '<<baselineRadius<<'\n';for(size_t i=0;i<miners.size();++i) out<<miners[i]<<' '<<minerWeapons[i]<<'\n';}
    else {std::ifstream in("hex-ordnance.ids");in>>secondBomb>>missileId>>baselineDamage>>baselineRadius;DWORD id;string weapon;while(in>>id>>weapon) {miners.push_back(id);minerWeapons.push_back(weapon);}}
}
}
DEFINE_HOOK(0x48928C,SF_TestOrdnanceArea,5) {
    auto wh=R->Base<WarheadTypeClass*>(0xC);
    if(wh && includes({"IvanWH","SF_HX_FireworksMainWH","SF_HX_FireworksWH","V3WH","V3EWH"},wh->ID)) {
        blasts.push_back({Unsorted::CurrentFrame,R->EDX<int>(),wh->CellSpread,*R->ECX<CoordStruct*>(),wh->ID});
        const auto& b=blasts.back();std::ofstream("hex-ordnance.log",std::ios::app)<<"blast "<<b.frame<<' '<<b.damage<<' '<<b.radius<<' '<<b.wh<<'\n';
    }
    return 0;
}
DEFINE_HOOK(0x55B4EB,SF_TestHexOrdnance,5) {
    if(GetFileAttributesA("Starfall.test")==INVALID_FILE_ATTRIBUTES) return 0;
    auto f=Unsorted::CurrentFrame;loading=GetFileAttributesA("reload.flag")!=INVALID_FILE_ATTRIBUTES;
    if(GetFileAttributesA("art-export.flag")!=INVALID_FILE_ATTRIBUTES) {
        if(f!=30) return 0;
        CCFileClass input("HexBrawl.art.ini"),output("artmd.ini");CCINIClass::INI_Art.ReadCCFile(&input);CCINIClass::INI_Art.WriteCCFile(&output);
        check(true,"prepared complete art rules");TerminateProcess(GetCurrentProcess(),0);return 0;
    }
    auto owner=HouseClass::FindByPlayerAt(HouseClass::PlayerAtA),foe=HouseClass::FindByPlayerAt(HouseClass::PlayerAtB);
    if(loading && miners.empty()) {
        ids(false);settle();
        check(bombSnapshots.contains(secondBomb) && !ActionSequence::timers.queue.empty(),"unexploded bomb and ongoing Lua sequence restored");
        for(size_t i=0;i<miners.size();++i) check(string(find(miners[i])->GetWeapon(0)->WeaponType->ID)==minerWeapons[i],"miner lifetime choice survives load");
        check(UnitHealth::read(find(missileId)).maximum==find(missileId)->GetTechnoType()->Strength*2,"rocket health does not double again on load");
    }
    if(!loading && f==30) {
        check(Research::options.size()==sectionKeys("Starfall.ResearchTypes").size(),"complete HexBrawl catalog loaded");
        licenses(owner,foe);newProjects(owner,foe);minerChecks(owner,foe);rocketChecks(owner,foe);
        auto ivan=place("IVAN",owner);planterId=ivan->UniqueID;
        auto oldBomb=plant(ivan,foe);
        auto base=plant(ivan,foe);bomb(base)->Detonate();
        check(!blasts.empty() && blasts.back().wh=="IvanWH","ordinary native bomb explosion");
        baselineDamage=blasts.back().damage;baselineRadius=blasts.back().radius;
        unlock(owner,"sf_hx_ivanboom",Research::Rarity::Common);unlock(owner,"sf_hx_fireworks",Research::Rarity::Rare);settle();
        bomb(oldBomb)->Detonate();
        check(blasts.back().wh=="IvanWH" && ActionSequence::timers.queue.empty(),"research leaves previously planted ordinary bombs unchanged");
        check(ivan->GetWeapon(0)->WeaponType==WeaponTypeClass::Find("SF_HX_FireworksBomber"),"research replaces Ivan planting weapon");
        ivan->Veterancy.Veterancy=2;settle();
        check(ivan->GetWeapon(0)->WeaponType==WeaponTypeClass::Find("SF_HX_FireworksBomberE"),"elite Ivan uses elite fireworks weapon");
        firstBomb=plant(ivan,foe);
        ivan->Veterancy.Veterancy=0;settle();
        secondBomb=plant(ivan,foe);bomb(secondBomb)->DetonationFrame=75;
        auto disarmed=plant(ivan,foe);bomb(disarmed)->Disarm();
        check(!bombSnapshots.contains(disarmed) && ActionSequence::timers.queue.empty(),"disarmed bomb does not schedule fireworks");
        check(bombSnapshots.contains(firstBomb) && bombSnapshots.at(firstBomb).scale==200,"bomb snapshots blast modifiers at planting");
        for(auto u:TechnoClass::Array) if(alive(u)) {u->QueueMission(Mission::Sleep,false);u->NextMission();}
    }
    if(!loading && f==45) {
        // Isolate the two planted bombs from Ivan's native Explodes=yes death bomb.
        auto type=find(planterId)->GetTechnoType();auto explodes=type->Explodes;type->Explodes=false;
        int damage=100000;find(planterId)->ReceiveDamage(&damage,0,WarheadTypeClass::Find("AP"),nullptr,true,false,foe);type->Explodes=explodes;
        check(!alive(find(planterId)),"planter killed before bomb detonation");
    }
    if(!loading && f==50) {
        bomb(firstBomb)->Detonate();
        check(blasts.back().wh=="SF_HX_FireworksMainWH" && blasts.back().damage==baselineDamage*2 && blasts.back().radius==baselineRadius*2,"planted bomb retains double damage and radius after planter death");
        check(ActionSequence::timers.queue.size()==1 && ActionSequence::casts.begin()->second.owner==owner->ArrayIndex,"actual detonation schedules independent Lua fireworks");
    }
    if(!loading && f==60) {
        int count=0;for(const auto& b:blasts) if(b.wh=="SF_HX_FireworksWH") {++count;check(b.damage==static_cast<int>(baselineDamage*0.4) && b.frame==50+count*3,"fireworks use configured damage and three frame spacing");}
        check(count==3,"exactly three of eight bursts before save");ids(true);
        check(ScenarioClass::SaveGame("Saved Games\\HexOrdnance.sav",L"Hex Ordnance"),"saved mid fireworks");
    }
    if(f==110) {
        int count=0;for(const auto& b:blasts) if(b.wh=="SF_HX_FireworksWH") ++count;
        check(count==(loading?13:16) && ActionSequence::timers.queue.empty() && ActionSequence::casts.empty(),"eight bursts per bomb finish without recursion or timer leaks");
        if(loading) {std::ifstream in("hex-ordnance.tail");string expected((std::istreambuf_iterator<char>(in)),{});check(tail()==expected,"save load preserves exact random positions timing and damage");}
        else std::ofstream("hex-ordnance.tail")<<tail();
        check(bombSnapshots.empty() && bombImpacts.empty() && spreadScopes.empty(),"ordnance state fully retired");
        if(!loading) psychicAndGrowth(owner,foe);
        auto wh=WarheadTypeClass::Find("SF_HX_FireworksMainWH");
        auto victim=place("MTNK",foe);
        auto at=victim->GetCoords();
        auto firer=place("MTNK",owner);
        auto projectile=BulletTypeClass::Find("Invisible")->CreateBullet(victim,firer,1,wh,100,false);
        check(projectile!=nullptr,"scripted projectile created");
        firer->UnInit();
        projectile->SetLocation(at);projectile->Detonate(at);projectile->UnInit();
        check(ActionSequence::timers.queue.size()==1 && ActionSequence::casts.begin()->second.owner==owner->ArrayIndex,
              "direct hit and area damage invoke one warhead callback");
        CellStruct point=CellClass::Coord2Cell(at);
        CellClass* empty=nullptr;
        for(short x=point.X+1;x<point.X+20 && !empty;++x) {
            CellStruct candidate{x,point.Y};
            auto cell=MapClass::Instance.TryGetCellAt(candidate);
            if(cell && MapClass::Instance.IsWithinUsableArea(candidate,false) && !cell->FirstObject) empty=cell;
        }
        check(empty!=nullptr,"empty test cell found");
        at=empty->GetCoords();
        MapClass::DamageArea(at,10,nullptr,wh,false,nullptr);
        MapClass::DamageArea(at,20,nullptr,wh,false,owner);
        check(ActionSequence::timers.queue.size()==3 && ActionSequence::casts.rbegin()->second.point==CellClass::Coord2Cell(at) &&
              std::next(ActionSequence::casts.rbegin())->second.owner==HouseClass::FindNeutral()->ArrayIndex,
              "two same-frame warhead detonations on empty ground invoke separate callbacks");

        TerminateProcess(GetCurrentProcess(),0);
    }
    return 0;
}
