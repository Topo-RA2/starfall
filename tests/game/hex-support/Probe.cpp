#include "../../../src/Starfall.cpp"

namespace {
const std::set<string> projects={"sf_hx_superbodyarmor","sf_hx_huatuoscroll","sf_hx_chaosdestiny","sf_hx_epictransmutation","sf_hx_raretransmutation"};
DWORD infantryId=0,newId=0,vehicleId=0,enemyId=0;
void check(bool ok,const char* label) {
    std::ofstream("hex-support.log",std::ios::app)<<(ok?"PASS ":"FAIL ")<<label<<'\n';
    if(!ok) TerminateProcess(GetCurrentProcess(),2);
}
TechnoClass* find(DWORD id) {for(auto u:TechnoClass::Array) if(u->UniqueID==id) return u;return nullptr;}
TechnoClass* place(const char* id,HouseClass* owner) {
    auto type=TechnoTypeClass::Find(id);check(type!=nullptr,"fixture type");
    auto u=static_cast<TechnoClass*>(type->CreateObject(owner));check(u!=nullptr,"fixture created");
    bool placed=false;
    for(short y=45;y<90 && !placed;y+=5) for(short x=45;x<90 && !placed;x+=5) {
        CellStruct c{x,y};
        if(BuildingSpawn::available(BuildingTypeClass::Find("NALASR"),owner,c))
            placed=u->Unlimbo(MapClass::Instance.GetCellAt(c)->GetCoords(),DirType::North);
    }
    check(placed,"fixture placed");u->QueueMission(Mission::Sleep,false);u->NextMission();return u;
}
void settle() {expireWarheadGrants();refreshAuras();dispatch();tickGains();tickStates();dispatch();tickIntervals();dispatch();}
void persistIds(bool load) {
    if(load) std::ifstream("hex-support.ids")>>infantryId>>newId>>vehicleId>>enemyId;
    else std::ofstream("hex-support.ids")<<infantryId<<' '<<newId<<' '<<vehicleId<<' '<<enemyId;
}
}
DEFINE_HOOK(0x55B4EB,SF_TestHexSupport,5) {
    if(GetFileAttributesA("Starfall.test")==INVALID_FILE_ATTRIBUTES) return 0;
    const int f=Unsorted::CurrentFrame;
    const bool loading=GetFileAttributesA("reload.flag")!=INVALID_FILE_ATTRIBUTES;
    auto owner=HouseClass::FindByPlayerAt(HouseClass::PlayerAtA),foe=HouseClass::FindByPlayerAt(HouseClass::PlayerAtB);
    const int h=owner->ArrayIndex;
    if(!loading && f==30) {
        Research::ensureReady();
        int source=-1;
        for(int i=0;i<int(Research::options.size());++i) {
            auto& o=Research::options[i];
            if(projects.contains(o.id)) {
                check(Research::available(i),"new project parsed and valid");
                if(o.id=="sf_hx_raretransmutation") source=i;
            } else {
                // Isolate this task while retaining the complete production config.
                o.countries={"__test_disabled"};
                WritePrivateProfileStringA(o.id.c_str(),"Countries","__test_disabled",configPath);
            }
        }
        check(source>=0 && skills.at("sf_hx_huatuoscroll_p").interval==30,"five projects and 30 frame healing configured");
        Research::EffectSpec invalid;invalid.effect=Research::Effect::RandomResearch;invalid.reward.count=0;
        check(!Research::Detail::validateEffect(invalid),"invalid reward count rejected");
        invalid.reward.count=1;invalid.reward.rarity=Research::Rarity::Invalid;
        check(!Research::Detail::validateEffect(invalid),"invalid reward rarity rejected");
        infantryId=place("E2",owner)->UniqueID;enemyId=place("E2",foe)->UniqueID;vehicleId=place("MTNK",owner)->UniqueID;
        // Choose a synchronized stream that first draws EpicTransmutation from
        // {HuaTuoScroll,EpicTransmutation}, then ChaosDestiny from {Armor,Chaos}.
        bool chosen=false;
        for(unsigned seed=1;seed<1000 && !chosen;++seed) {
            const uint64_t state=0x9E3779B97F4A7C15ULL*seed;
            scriptTimers.rng=state;
            if(Research::Detail::randomIndex(2)==1 && Research::Detail::randomIndex(2)==1) {scriptTimers.rng=state;chosen=true;}
        }
        check(chosen,"deterministic chain fixture");
        Research::points[h]=200;Research::savedOffers[h]={"sf_hx_raretransmutation"};Research::clearOfferCache();
        check(Research::purchase(h,source,owner->Type->ID),"purchased random reward project");
        const auto& order=Research::unlockOrder[h];
        check(Research::unlocked[h]==projects && order.size()==5 && order[0]=="sf_hx_raretransmutation" &&
            order[1]=="sf_hx_epictransmutation" && order[2]=="sf_hx_chaosdestiny" &&
            Research::balance(h)==100 && Research::completedRounds[h]==1 && !Research::unlocked.contains(foe->ArrayIndex),
            "native reward chain excludes self and duplicates with one payment and round");
        settle();
        auto infantry=find(infantryId),enemy=find(enemyId),vehicle=find(vehicleId);
        check(std::abs(infantry->ArmorMultiplier-3)<1e-6 && std::abs(static_cast<FootClass*>(infantry)->SpeedMultiplier-.85)<1e-6 &&
            enemy->ArmorMultiplier==1 && vehicle->ArmorMultiplier==1,"infantry armor and speed apply only to owner infantry");
        auto later=place("E2",owner);newId=later->UniqueID;settle();
        check(std::abs(later->ArmorMultiplier-3)<1e-6,"new infantry inherits research");
        later->SetOwningHouse(foe,false);settle();
        check(later->ArmorMultiplier==1 && static_cast<FootClass*>(later)->SpeedMultiplier==1,"capture withdraws old owner modifiers");
        later->SetOwningHouse(owner,false);settle();
        for(auto id:{infantryId,newId,vehicleId,enemyId}) {auto u=find(id);u->Health=u->EstimatedHealth=1;}
        persistIds(false);
    }
    if(f<45) return 0;
    if(loading) persistIds(true);
    if(!infantryId) return 0;
    if(!loading && f==45) check(ScenarioClass::SaveGame("Saved Games\\HexSupport.sav",L"Hex support"),"saved between healing ticks");
    if(f==59 || f==60 || f==90 || f==180) {
        settle();auto infantry=find(infantryId);auto later=find(newId);
        const int expected=std::min(infantry->GetTechnoType()->Strength,1+25*((f-30)/30));
        std::ofstream("hex-support.log",std::ios::app)<<"frame "<<f<<" hp "<<infantry->Health<<" expected "<<expected<<'\n';
        check(infantry->Health==expected && later->Health==expected,"healing exact interval amount and maximum");
        check(find(vehicleId)->Health==1 && find(enemyId)->Health==1,"healing excludes vehicles and enemy infantry");
        check(Research::unlocked[h]==projects && Research::unlockOrder[h].size()==5 && Research::completedRounds[h]==1 &&
            std::abs(infantry->ArmorMultiplier-3)<1e-6 && std::abs(static_cast<FootClass*>(infantry)->SpeedMultiplier-.85)<1e-6,
            "research and modifiers persist without duplicate rewards");
    }
    if(f==190) {
        const auto rng=scriptTimers.rng;
        check(Research::grantRandom(h,owner->Type->ID)<0 && scriptTimers.rng==rng,"exhausted rewards consume no random draw");
        check(Research::grantRandom(foe->ArrayIndex,foe->Type->ID)>=0,"free grant supports reward projects");
        std::string result;
        for(const auto& id:Research::unlockOrder[foe->ArrayIndex]) result+=id+'\n';
        result+=std::to_string(scriptTimers.rng);
        if(loading) {
            std::ifstream file("hex-support.next");std::string expected((std::istreambuf_iterator<char>(file)),{});
            check(result==expected,"random chain resumes same stream after load");
        } else std::ofstream("hex-support.next")<<result;
        check(true,"completed infantry and random reward projects");TerminateProcess(GetCurrentProcess(),0);
    }
    return 0;
}
