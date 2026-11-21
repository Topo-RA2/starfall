#include "../../../src/Starfall.cpp"
#include <EventClass.h>

namespace {
DWORD giId=0,plantId=0,yardId=0;
int giftCount=0,basePrice=0,basePower=0;
bool load=false;
void check(bool ok,const char* label) {
    std::ofstream("hex-economy.log",std::ios::app)<<(ok?"PASS ":"FAIL ")<<label<<'\n';
    if(!ok) TerminateProcess(GetCurrentProcess(),2);
}
TechnoClass* find(DWORD id) {for(auto u:TechnoClass::Array) if(u->UniqueID==id) return u;return nullptr;}
int sentries(HouseClass* h) {int n=0;for(auto b:BuildingClass::Array) if(alive(b) && b->IsOnMap && b->Owner==h && b->Type==BuildingTypeClass::Find("NALASR")) ++n;return n;}
CellStruct empty(BuildingTypeClass* type,HouseClass* owner) {
    for(short y=40;y<95;y+=4) for(short x=40;x<95;x+=4) {
        CellStruct c{x,y};if(BuildingSpawn::available(type,owner,c)) return c;
    }
    check(false,"fixture space");return {};
}
TechnoClass* place(const char* id,HouseClass* owner) {
    auto type=TechnoTypeClass::Find(id);check(type!=nullptr,"fixture type exists");
    auto u=static_cast<TechnoClass*>(type->CreateObject(owner));check(u!=nullptr,"fixture object created");
    auto probe=type->WhatAmI()==AbstractType::BuildingType?static_cast<BuildingTypeClass*>(type):BuildingTypeClass::Find("NALASR");
    auto cell=empty(probe,owner);
    check(u->Unlimbo(MapClass::Instance.GetCellAt(cell)->GetCoords(),DirType::North),"fixture placed");
    if(u->WhatAmI()==AbstractType::Building) {
        auto b=static_cast<BuildingClass*>(u);b->BeginMode(BStateType::Idle);b->Place(false);
    }
    u->QueueMission(Mission::Sleep,false);u->NextMission();return u;
}
void settle() {expireWarheadGrants();refreshAuras();dispatch();tickGains();tickStates();dispatch();}
void unlock(HouseClass* owner,const char* id,Research::Rarity rarity) {
    int index=-1;for(int i=0;i<int(Research::options.size());++i) if(Research::options[i].id==id) index=i;
    check(index>=0 && Research::available(index) && Research::options[index].rarity==rarity,"new project parsed and valid");
    Research::unlocked[owner->ArrayIndex].insert(id);Research::clearOfferCache();discoverAuras=true;
}
double rate(TechnoClass* u) {return unitFactor(u->UniqueID,fireRateBase,fireRateState);}
void powerThresholds(HouseClass* owner,HouseClass* foe) {
    auto normal=place("NAFLAK",owner),giant=place("SF_HX_GiantTESLA",owner),zero=place("NALASR",owner),enemy=place("NAFLAK",foe);
    for(int spare:{-500,0,499,500,999,1000,1499,1500,1999,2000,2499,2500,10000}) {
        owner->PowerOutput=1000+spare;owner->PowerDrain=1000;settle();
        const double expected=1+std::clamp(spare/500,0,5)*0.1;
        check(std::abs(rate(normal)-expected)<1e-6 && std::abs(rate(giant)-expected)<1e-6,"power surplus exact threshold and cap");
        check(rate(zero)==1 && rate(enemy)==1,"zero consumption and other owner excluded");
    }
    check(normal->SetOwningHouse(foe,false),"defense captured");settle();check(rate(normal)==1,"power bonus withdrawn on capture");
    owner->UpdatePower();foe->UpdatePower();
}
struct CellLess {bool operator()(CellStruct a,CellStruct b) const {return a.Y!=b.Y?a.Y<b.Y:a.X<b.X;}};
std::set<CellStruct,CellLess> randomRewardCrates() {
    std::set<CellStruct,CellLess> out;
    for(const auto& c:MapClass::Instance.Crates) if(c.CrateTimer.HasTimeLeft()) {
        auto cell=MapClass::Instance.TryGetCellAt(c.Location);
        if(cell && cell->OverlayData==static_cast<BYTE>(randomCrate)) out.insert(c.Location);
    }
    return out;
}
void killWith(TechnoClass* killer,TechnoClass* victim) {
    int damage=100000;victim->ReceiveDamage(&damage,0,WarheadTypeClass::Find("AP"),killer,true,false,killer->Owner);settle();
}
void crates(HouseClass* owner,HouseClass* foe) {
    auto magnet=place("TELE",owner),target=place("MTNK",foe);auto pos=target->GetMapCoords();
    auto before=randomRewardCrates();killWith(magnet,target);auto after=randomRewardCrates();
    check(after.size()==before.size()+1,"magnetron native enemy kill drops random reward crate");
    CellStruct added{};for(auto c:after) if(!before.contains(c)) added=c;
    check(std::abs(added.X-pos.X)<=3 && std::abs(added.Y-pos.Y)<=3,"crate uses victim location");
    auto collector=static_cast<InfantryClass*>(place("E2",owner));
    // Random rewards may also damage/destroy the collector; do not assume a rank
    // increase or access its pointer after the native pickup resolves.
    check(MapClass::Instance.GetCellAt(added)->CollectCrate(collector) && !randomRewardCrates().contains(added),"native random reward crate collected");
    before=randomRewardCrates();killWith(magnet,place("MTNK",owner));
    check(randomRewardCrates()==before,"friendly kill gives no crate");
    auto neutral=HouseClass::FindNeutral();check(neutral!=nullptr,"neutral fixture");
    killWith(magnet,place("MTNK",neutral));check(randomRewardCrates()==before,"neutral kill gives no crate");
    killWith(place("MTNK",owner),place("MTNK",foe));check(randomRewardCrates()==before,"other killer gives no crate");
}
void buildAI(HouseClass* owner,const char* id,bool success=true) {
    auto yard=static_cast<BuildingClass*>(find(yardId));auto type=BuildingTypeClass::Find(id);
    auto b=static_cast<BuildingClass*>(type->CreateObject(owner));
    auto at=success?empty(type,owner):yard->GetMapCoords();int before=sentries(owner);
    const bool human=owner->IsHumanPlayer;owner->IsHumanPlayer=false;
    const auto result=yard->KickOutUnit(b,at);owner->IsHumanPlayer=human;settle();
    std::ofstream("hex-economy.log",std::ios::app)<<"build "<<id<<" result "<<int(result)<<" gifts "<<sentries(owner)-before<<'\n';
    if(success) check(result==KickOutResult::Succeeded && sentries(owner)==before+1,"native factory placement gifts exactly one sentry");
    else {check(result!=KickOutResult::Succeeded && sentries(owner)==before,"failed placement gives no sentry");b->UnInit();}
}
void buildHuman(HouseClass* owner,const char* id="NAFLAK") {
    auto type=BuildingTypeClass::Find(id);auto yard=static_cast<BuildingClass*>(find(yardId));
    auto factory=static_cast<FactoryClass*>(YRMemory::AllocateChecked(sizeof(FactoryClass)));
    reinterpret_cast<void(__thiscall*)(FactoryClass*)>(0x4C98B0)(factory);factory->Owner=owner;
    auto b=static_cast<BuildingClass*>(type->CreateObject(owner));factory->SetObject(b);
    owner->SetPrimaryFactory(factory,AbstractType::BuildingType,false,BuildCat::Combat);
    yard->Factory=factory;
    auto at=empty(type,owner);int before=sentries(owner);
    auto blocked=yard->GetMapCoords();
    reinterpret_cast<bool(__thiscall*)(HouseClass*,AbstractType,int,bool,const CellStruct*)>(0x4FB0E0)(owner,AbstractType::BuildingType,type->ArrayIndex,false,&blocked);
    settle();check(!b->IsOnMap && b->InLimbo && sentries(owner)==before,"failed placement gives no sentry");
    reinterpret_cast<bool(__thiscall*)(HouseClass*,AbstractType,int,bool,const CellStruct*)>(0x4FB0E0)(owner,AbstractType::BuildingType,type->ArrayIndex,false,&at);
    settle();check(b->IsOnMap && !b->InLimbo && sentries(owner)==before+1,"human factory placement gifts exactly one sentry");
    yard->Factory=nullptr;owner->SetPrimaryFactory(nullptr,AbstractType::BuildingType,false,BuildCat::Combat);
    // UnitFromFactory normally releases its completed factory through AbandonProduction.
    bool retained=false;for(auto liveFactory:FactoryClass::Array) if(liveFactory==factory) retained=true;
    if(retained) {factory->Object=nullptr;delete factory;}
}
void deploy(TechnoClass* u) {
    EventClass e(u->Owner->ArrayIndex,EventType::Deploy);e.Deploy.Whom=TargetClass(u);
    check(EventClass::OutList.Add(e),"native deployment command queued");
}
void prism(bool elite) {
    auto u=static_cast<InfantryClass*>(find(giId));settle();
    auto weapon=WeaponTypeClass::Find(elite?"SF_HX_PrismGIWeaponE":"SF_HX_PrismGIWeapon");
    check(u && u->IsDeployed() && u->GetWeapon(1)->WeaponType==weapon && std::abs(u->ArmorMultiplier-1.5)<1e-6,"deployed prism weapon and armor");
    auto native=u->Type->GetWeapon(1,elite).WeaponType;
    std::ofstream("hex-economy.log",std::ios::app)<<"GI original "<<native->ID<<" damage "<<native->Damage<<" range "<<native->Range<<" ROF "<<native->ROF<<" burst "<<native->Burst<<'\n';
    check(weapon->Damage>native->Damage && weapon->Damage<=native->Damage*1.5 && weapon->Range==native->Range+512 && weapon->ROF==native->ROF && weapon->Burst==native->Burst && weapon->IsLaser,"prism improves damage and range retaining native cadence");
    auto foe=HouseClass::FindByPlayerAt(HouseClass::PlayerAtB);auto target=place("E2",foe);int hp=target->Health;
    u->SetTarget(target);u->RearmTimer.Stop();
    auto facing=u->GetTargetDirection(target);u->PrimaryFacing.SetCurrent(facing);u->SecondaryFacing.SetCurrent(facing);
    auto bullet=u->Fire(target,1);
    check(bullet!=nullptr,"native prism creates projectile");
    auto at=target->GetCoords();bullet->SetLocation(at);bullet->Detonate(at);bullet->UnInit();settle();
    check(target->Health<hp,"native prism shot deals damage");u->SetTarget(nullptr);
}
void writeIds() {std::ofstream("hex-economy.ids")<<giId<<' '<<plantId<<' '<<yardId<<' '<<giftCount<<' '<<basePrice<<' '<<basePower;}
void readIds() {std::ifstream("hex-economy.ids")>>giId>>plantId>>yardId>>giftCount>>basePrice>>basePower;}
}
DEFINE_HOOK(0x55B4EB,SF_TestHexEconomy,5) {
    if(GetFileAttributesA("Starfall.test")==INVALID_FILE_ATTRIBUTES) return 0;
    int f=Unsorted::CurrentFrame;load=GetFileAttributesA("reload.flag")!=INVALID_FILE_ATTRIBUTES;
    if(GetFileAttributesA("art-export.flag")!=INVALID_FILE_ATTRIBUTES) {
        if(f!=30) return 0;
        CCFileClass input("HexBrawl.art.ini"),output("artmd.ini"),rulesOut("hex-economy-baseline.rules.ini");
        CCINIClass::INI_Art.ReadCCFile(&input);CCINIClass::INI_Art.WriteCCFile(&output);CCINIClass::INI_Rules->WriteCCFile(&rulesOut);
        check(true,"prepared complete art rules");TerminateProcess(GetCurrentProcess(),0);return 0;
    }
    auto owner=HouseClass::FindByPlayerAt(HouseClass::PlayerAtA),foe=HouseClass::FindByPlayerAt(HouseClass::PlayerAtB);
    if(load) {
        if(f<160) return 0;readIds();settle();owner->UpdatePower();
        check(sentries(owner)==giftCount,"load does not repeat construction gifts");
        check(InfantryTypeClass::Find("E1")->GetActualCost(owner)==basePrice*2 && powerContribution(static_cast<BuildingClass*>(find(plantId)),basePower,0)==(basePower*3+1)/2 && owner->PowerOutput>=(basePower*3+1)/2,"power and price research survive load");
        prism(true);powerThresholds(owner,foe);buildAI(owner,"NAFLAK");
        check(true,"all five projects persist through save");TerminateProcess(GetCurrentProcess(),0);return 0;
    }
    if(f==30) {
        Research::ensureReady();check(Research::options.size()==85,"complete HexBrawl catalog loaded");
        check(!skills.contains("invalidliveevent") && !skills.contains("invalidliveaction"),"live formula variables rejected in events");
        auto plant=static_cast<BuildingClass*>(place("GAPOWR",owner));plantId=plant->UniqueID;basePower=plant->GetPowerOutput();
        auto soviet=static_cast<BuildingClass*>(place("NAPOWR",owner));int sovietPower=soviet->GetPowerOutput();
        auto other=static_cast<BuildingClass*>(place("GAPOWR",foe));int otherPower=other->GetPowerOutput();
        auto preplaced=place("TESLA",owner);yardId=place("NACNST",owner)->UniqueID;
        giId=place("E1",owner)->UniqueID;basePrice=InfantryTypeClass::Find("E1")->GetActualCost(owner);
        int otherPrice=InfantryTypeClass::Find("E1")->GetActualCost(foe),before=sentries(owner);
        owner->UpdatePower();foe->UpdatePower();const int gridBefore=owner->PowerOutput,otherGrid=foe->PowerOutput;
        unlock(owner,"sf_hx_buyonegetone",Research::Rarity::Common);unlock(owner,"sf_hx_powerturbine",Research::Rarity::Common);
        unlock(owner,"sf_hx_trashtotreasure",Research::Rarity::Rare);unlock(owner,"sf_hx_powersurplus",Research::Rarity::Common);unlock(owner,"sf_hx_prismgi",Research::Rarity::Epic);
        settle();owner->UpdatePower();
        check(owner->PowerOutput==gridBefore+(basePower+1)/2 && soviet->GetPowerOutput()==sovietPower && other->GetPowerOutput()==otherPower && foe->PowerOutput==otherGrid,"allied plant output increased only for researcher");
        check(sentries(owner)==before,"existing buildings give no research gift");
        check(InfantryTypeClass::Find("E1")->GetActualCost(owner)==basePrice*2 && InfantryTypeClass::Find("E1")->GetActualCost(foe)==otherPrice,"GI price doubled only for researcher");
        check(find(giId)->GetWeapon(0)->WeaponType==InfantryTypeClass::Find("E1")->GetWeapon(0,false).WeaponType,"undeployed GI keeps original weapon");
        powerThresholds(owner,foe);crates(owner,foe);
        before=sentries(owner);auto site=empty(BuildingTypeClass::Find("TESLA"),owner);
        check(BuildingSpawn::create(BuildingTypeClass::Find("TESLA"),owner,MapClass::Instance.GetCellAt(site)->GetCoords(),0),"skill generated tower fixture");
        check(preplaced->SetOwningHouse(foe,false) && preplaced->SetOwningHouse(owner,false),"capture fixture");settle();
        check(sentries(owner)==before,"spawn and capture give no construction gifts");
        buildAI(owner,"TESLA");buildAI(owner,"NAFLAK");buildHuman(owner);
        unlock(owner,"sf_hx_superdefense",Research::Rarity::Epic);settle();
        buildAI(owner,"SF_HX_GiantTESLA");buildHuman(owner,"SF_HX_GiantNAFLAK");
        for(auto u:TechnoClass::Array) if(u->UniqueID!=giId && u->UniqueID!=plantId && u->UniqueID!=yardId && alive(u)) {u->Deactivate();u->QueueMission(Mission::Sleep,false);u->NextMission();}
        deploy(find(giId));
    }
    if(f==80) {prism(false);find(giId)->Veterancy.Veterancy=2;settle();prism(true);deploy(find(giId));}
    if(f==115) {
        auto u=static_cast<InfantryClass*>(find(giId));settle();
        check(!u->IsDeployed() && u->GetWeapon(0)->WeaponType==u->Type->GetWeapon(0,true).WeaponType && u->GetWeapon(1)->WeaponType==u->Type->GetWeapon(1,true).WeaponType,"undeployment restores both native slots");deploy(u);
    }
    if(f==150) {
        prism(true);giftCount=sentries(owner);writeIds();
        check(ScenarioClass::SaveGame("Saved Games\\HexEconomy.sav",L"Hex Economy"),"saved");TerminateProcess(GetCurrentProcess(),0);
    }
    return 0;
}
