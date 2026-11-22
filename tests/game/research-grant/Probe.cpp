#include "../../../src/Starfall.cpp"
#include <MessageListClass.h>

namespace {
void check(bool ok,const char* label) {
    std::ofstream("research-grant.log",std::ios::app)<<(ok?"PASS ":"FAIL ")<<label<<'\n';
    if(!ok) TerminateProcess(GetCurrentProcess(),2);
}
bool checkExternalList(int house,const char* expected,uint32_t bytes) {
    using ClientCheck=int (__cdecl*)(int32_t,const char*,uint32_t);
    static HMODULE client=LoadLibraryW(L".\\ResearchApiClient.dll");
    static auto verify=client?reinterpret_cast<ClientCheck>(GetProcAddress(client,"CheckOwnedResearch")):nullptr;
    return verify && verify(house,expected,bytes);
}
bool checkExternalDisplay(int house,const char* firstId,uint32_t count) {
    using ClientCheck=int (__cdecl*)(int32_t,const char*,uint32_t);
    static const auto client=GetModuleHandleW(L"ResearchApiClient.dll");
    static const auto verify=client?reinterpret_cast<ClientCheck>(GetProcAddress(client,"CheckResearchDisplay")):nullptr;
    return verify && verify(house,firstId,count);
}
bool ownerReward(HouseClass* house,const char* rarity) {
    ScriptFields fields{{"action","grantrandomresearch"},{"action.target","owner"}};
    if(rarity) fields["action.rarity"]=rarity;
    Skill step{};step.id="probe-owner-reward";step.trigger="onmaptrigger";
    check(ActionSequence::prepare(fields) && readAction(step,step.id,&fields),"owner action parses");
    Event event{};event.trigger="onmaptrigger";event.owner=house->ArrayIndex;
    Objects objects;
    return apply(step.actions.front(),event,nullptr,objects,0);
}
void killEvent(TechnoClass* unit,TechnoClass* target) {
    capture("onkill",unit,target,unit,target,0);dispatch();tickGains();tickStates();dispatch();
}
}
DEFINE_HOOK(0x55B4EB,SF_TestResearchGrant,5) {
    if(GetFileAttributesA("Starfall.test")==INVALID_FILE_ATTRIBUTES) return 0;
    const int frame=Unsorted::CurrentFrame;
    const bool loading=GetFileAttributesA("reload.flag")!=INVALID_FILE_ATTRIBUTES;
    if(frame!=20 && frame!=25 && frame!=35 && frame!=50) return 0;
    if(loading && frame<=25) return 0;
    Research::ensureReady();
    auto house=HouseClass::FindByPlayerAt(HouseClass::PlayerAtA);
    auto foe=HouseClass::FindByPlayerAt(HouseClass::PlayerAtB);
    TechnoClass *unit=nullptr,*target=nullptr;
    for(auto candidate:UnitClass::Array) if(candidate && !_stricmp(candidate->Type->ID,"MTNK")) {
        if(candidate->Owner==house && !unit) unit=candidate;
        if(candidate->Owner==foe && !target) target=candidate;
    }
    // The test range may give the other house a different tank type.
    if(!target) for(auto candidate:UnitClass::Array) if(candidate && candidate->Owner==foe) {target=candidate;break;}
    check(unit && target,"fixture units");
    const int h=house->ArrayIndex,v=foe->ArrayIndex;
    if(frame==20) {
        check(checkExternalList(-1,nullptr,0) && checkExternalList(HouseClass::Array.Count,nullptr,0) &&
            checkExternalList(h,"\0",2),"external DLL distinguishes missing house and empty ownership");
        check(checkExternalDisplay(h,nullptr,0),"external display API distinguishes empty research");
        check(skills.contains("rarereward") && skills.contains("epicreward") && skills.contains("scriptreward") &&
            !skills.contains("badrarity") && !skills.contains("badrarityfield"),"random research configuration validated");
        // Apply the boost to the actual target type supplied by the base map.
        auto& boost=Research::options[7];boost.types={lower(target->GetTechnoType()->ID)};
        WritePrivateProfileStringA("EpicBoost","TargetTypes",target->GetTechnoType()->ID,configPath);
        Research::points[h]=100;
        auto board=Research::offers(h,house->Type->ID);
        check(board.size()==3 && Research::refresh(h,house->Type->ID,0,board.front()),"prepare ordinary refresh");
        int money=house->Available_Money();
        grantToUnit(unit,PassiveGrant{{"rarereward"},-1});killEvent(unit,target);
        check(Research::unlocked[h].size()==1 && house->Available_Money()==money+25 && Research::balance(h)==100 &&
            Research::completedRounds.empty() && Research::slotUsed(h,0),"INI rare reward is free and preserves ordinary progress");
        auto acquired=Research::completedProjects(h).front();
        const auto notice=std::wstring(house->UIName)+L" 已研究「"+Research::options[acquired].name+L"」。";
        int notices=0;for(const auto& text:MessageListClass::Instance.MessageBuffers) if(notice==text) ++notices;
        check(notices==1,"random research announced once");
        grantToUnit(unit,PassiveGrant{{"epicreward","scriptreward","choicereward"},-1});killEvent(unit,target);
        check(Research::owns(v,"epicboost") && ownsPassive(target,"rewardboost") &&
            std::abs(target->FirepowerMultiplier-1.25)<0.0001,"target owner receives live research passive");
        check(Research::owns(v,"insanea") && scriptTimers.state.at({unit->UniqueID,"scriptreward","checked"}).integer==1,
            "Lua exact rarity ignores ordinary zero weights and rejects duplicates");
        check(ownerReward(house,"Rare") && Research::owns(h,"rarea") && Research::owns(h,"rareb") &&
            Research::unlocked[h].size()==2,"owner context grants remaining rare project without a unit");
        auto random=ScenarioClass::Instance->Random;
        const auto stream=scriptTimers.rng;
        check(!ownerReward(house,"Rare") && memcmp(&random,&ScenarioClass::Instance->Random,sizeof(random))==0 && scriptTimers.rng==stream,
            "ineligible pool fails without RNG consumption");
        check(!Research::owns(h,"wrongcountry") && !Research::owns(h,"missingprerequisite") && !Research::owns(h,"invalideffect"),
            "country prerequisite and effect restrictions enforced");
    }
    if(frame==20 || frame==35) {
        const auto tickets=Research::pendingFreeRewards(v);
        check(tickets.size()==2 && Research::pendingFreeRewards(h).empty(),"only the target owner sees both pending choices");
        std::string signature;
        for(auto id:tickets) {
            signature+=std::to_string(id);
            for(int i:Research::freeOptions(id,v)) signature+=","+Research::options[i].id;
            signature+=";";
        }
        if(frame==20) std::ofstream("research-grant.cards")<<signature;
        else {
            std::string expected;std::ifstream("research-grant.cards")>>expected;
            check(signature==expected,"scoped and unrestricted free cards survive save and load");
        }
    }
    if(frame==25) {
        check(ScenarioClass::SaveGame("Saved Games\\ResearchGrant.sav",L"Research grant"),"saved");
    }
    if(frame==35) {
        constexpr char playerIds[]="rarea\0rareb\0",opponentIds[]="epicboost\0insanea\0";
        check(checkExternalList(h,playerIds,sizeof(playerIds)) && checkExternalList(v,opponentIds,sizeof(opponentIds)),
            "external DLL reads exact research IDs for each house");
        check(checkExternalDisplay(v,"epicboost",2),"external display API reads completed research and bounded icon");
        check(Research::owns(h,"rarea") && Research::owns(h,"rareb") && Research::owns(v,"epicboost") &&
            Research::owns(v,"insanea") && Research::unlocked[h].size()==2 && Research::unlocked[v].size()==2 &&
            Research::completedRounds.empty() && Research::balance(h)==100 && Research::slotUsed(h,0),"free research and round zero survive load");
        check(ownsPassive(target,"rewardboost") && std::abs(target->FirepowerMultiplier-1.25)<0.0001,"research passive survives load");
        auto tickets=Research::pendingFreeRewards(v);
        if(Research::freeOptions(tickets[0],v).size()!=1) std::swap(tickets[0],tickets[1]);
        auto scoped=Research::freeOptions(tickets[0],v),open=Research::freeOptions(tickets[1],v);
        check(scoped==vector<int>{4} && open.size()==3 && std::set<int>(open.begin(),open.end()).size()==3 &&
            std::all_of(open.begin(),open.end(),[](int i){return i>=0 && i<=4;}),"INI scope and default Lua pool use eligible distinct cards");
        check(!Research::claimFree(tickets[0],h,4) && !Research::claimFree(tickets[0],v,1) &&
            Research::claimFree(tickets[0],v,4) && !Research::claimFree(tickets[0],v,4),"free choice validates scope owner and single use");
        auto follow=Research::ticketFlag|Research::lastTicket;
        check(Research::freeOptions(follow,v)==vector<int>{2,3} && Research::claimFree(follow,v,2) &&
            Research::balance(v)==0 && Research::completedRounds.empty() && Research::pendingFreeRewards(v)==vector<Research::RewardId>{tickets[1]},
            "research effect queues a separate scoped choice at zero points");
        check(Research::grantFree(v,foe->Type->ID,{},Research::Rarity::Rare),"grant exact rare choice");
        auto rare=Research::freeOptions(Research::ticketFlag|Research::lastTicket,v);
        check(std::set<int>(rare.begin(),rare.end())==std::set<int>{5,6},"explicit rarity includes zero-weight eligible projects");
        const auto nativeRandom=ScenarioClass::Instance->Random;
        check(ownerReward(house,nullptr) && Research::unlocked[h].size()==3 && Research::completedRounds.empty(),"default All grants one additional project");
        check(memcmp(&nativeRandom,&ScenarioClass::Instance->Random,sizeof(nativeRandom))==0,"reward draw leaves native RNG unchanged");
        const auto id=Research::unlockOrder[h].back();
        if(loading) {
            string expected;std::ifstream("research-grant.next")>>expected;
            check(id==expected,"random research resumes same draw after load");
        } else std::ofstream("research-grant.next")<<id;
        const auto before=Research::unlocked;
        Research::enabled=false;
        check(!ownerReward(house,"All") && Research::unlocked==before,"research switch blocks free grants");
        Research::enabled=true;
        Research::allowAIResearch=true;
    }
    if(frame==50) {
        check(Research::unlocked[v].size()==5 && Research::balance(v)==0 && !Research::completedRounds.contains(v),
            "AI claims one pending choice without research points");
        check(true,"completed random research test");TerminateProcess(GetCurrentProcess(),0);
    }
    return 0;
}
