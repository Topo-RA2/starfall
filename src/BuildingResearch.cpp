#include "BuildingResearch.h"
#include "PassiveRules.h"
#include "ConfigText.h"
#include "ResearchInternal.h"
#include "UnitHealth.h"
#include <BuildingClass.h>
#include <HouseClass.h>
#include <InfantryClass.h>
#include <SessionClass.h>
#include <Unsorted.h>
#include <unordered_map>

using std::string;
using std::vector;
using UnitHealth::alive;
using Research::Detail::Runtime::eligibleHouse;

bool Research::Detail::Runtime::eligibleHouse(HouseClass* h) {
    return h && h->ArrayIndex>=0 && !h->Defeated && !h->IsObserver() && !h->IsNeutral();
}

namespace BuildingResearch {
using ConfigText::includes;
using ConfigText::number;
using ConfigText::split;
using PassiveRules::invalidField;
using PassiveRules::read;

namespace {
struct BuildingRules {
    RewardRule reward;
    int points=0,interval=0;
    bool producesPoints() const {return points>0 && interval>0;}
    bool needsTick() const {return producesPoints() || !reward.options.empty();}
};
std::unordered_map<BuildingTypeClass*,BuildingRules> buildingTypeRules;
BuildingTypeClass* const* scannedTypes=nullptr;
int scannedTypeCount=-1;
bool typeRegistryComplete=false;
bool hasConfiguredBuildingRules=false;

BuildingRules readBuildingRules(BuildingTypeClass* type) {
    BuildingRules rules;
    rules.reward.options=split(read(type->ID,"Starfall.ResearchOptions"));
    auto flag=[&](const char* key) {
        auto value=read(type->ID,key,"no");
        if(includes({"yes","true","1"},value)) return true;
        if(!includes({"no","false","0"},value)) invalidField(type->ID,key);
        return false;
    };
    rules.reward.engineer=flag("Starfall.ResearchOptions.AllowEngineerCapture");
    rules.reward.spy=flag("Starfall.ResearchOptions.AllowSpyInfiltration");
    rules.points=number(read(type->ID,"Starfall.ResearchPoints","0"));
    rules.interval=number(read(type->ID,"Starfall.ResearchInterval","150"));
    return rules;
}

const BuildingRules& rulesForType(BuildingTypeClass* type) {
    auto [it,fresh]=buildingTypeRules.try_emplace(type);
    if(fresh) {
        it->second=readBuildingRules(type);
        hasConfiguredBuildingRules|=it->second.needsTick();
    }
    return it->second;
}

bool needsBuildingScan() {
    if(hasConfiguredBuildingRules) return true;
    const auto& types=BuildingTypeClass::Array;
    if(scannedTypes!=types.Items || scannedTypeCount!=types.Count) {
        scannedTypes=types.Items;
        scannedTypeCount=types.Count;
        // Only a complete native type registry can prove no buildings use research.
        typeRegistryComplete=false;
        if(!types.Items || types.Count<=0) return true;
        for(int i=0;i<types.Count;++i) {
            auto type=types.Items[i];
            if(!type || type->ArrayIndex!=i || rulesForType(type).needsTick()) return true;
        }
        typeRegistryComplete=true;
    }
    return !typeRegistryComplete;
}
}

void clearTypeCaches() {
    buildingTypeRules.clear();
    scannedTypes=nullptr;
    scannedTypeCount=-1;
    typeRegistryComplete=false;
    hasConfiguredBuildingRules=false;
}
const RewardRule& rewardRule(BuildingTypeClass* type) {
    return rulesForType(type).reward;
}
void tickGlobalResearch() {
    auto& state=Research::globalIncome;
    unsigned frame=static_cast<unsigned>(Unsorted::CurrentFrame);
    if(state.initialized && (!state.periodicPoints || !state.intervalFrames || frame-state.lastFrame<state.intervalFrames)) return;
    std::set<int> players;
    auto add=[&](HouseClass* house) {if(eligibleHouse(house)) players.insert(house->ArrayIndex);};
    for(int slot=0;slot<8;++slot) add(HouseClass::FindByPlayerAt(HouseClass::PlayerAtA+slot));
    if(SessionClass::IsCampaign()) add(HouseClass::CurrentPlayer);
    if(players.empty()) return;
    if(!state.initialized) {
        for(int house:players) Research::award(house,state.initialPoints);
        state.initialized=true;state.lastFrame=frame;
    } else if(state.periodicPoints>0 && state.intervalFrames>0 && frame-state.lastFrame>=state.intervalFrames) {
        for(int house:players) Research::award(house,state.periodicPoints);
        state.lastFrame=frame;
    }
}
namespace {
void tickConfiguredBuildings() {
    for(auto* b:BuildingClass::Array) {
        if(!b || !b->Type) continue;
        const auto& rules=rulesForType(b->Type);
        if(!rules.reward.options.empty() && b->Owner && alive(b)) {
            auto [reward,added]=Research::buildingRewards.try_emplace(b->UniqueID,Research::BuildingReward{b->Owner->ArrayIndex,-1,false});
            if(!added && reward->second.owner!=b->Owner->ArrayIndex) {
                reward->second.owner=b->Owner->ArrayIndex;
                reward->second.authorized=-1;
                reward->second.source=Research::BuildingRewardSource::None;
            }
        }
        if(!rules.producesPoints()) continue;
        auto id=b->UniqueID;
        if(!eligibleHouse(b->Owner) || !alive(b) || !b->IsOnMap || b->InLimbo ||
            b->CurrentMission==Mission::Selling || b->CurrentMission==Mission::Construction) {
            Research::producers.erase(id); continue;
        }
        int owner=b->Owner->ArrayIndex;
        auto [entry,added]=Research::producers.try_emplace(id,Research::Producer{owner,0});
        auto& timer=entry->second;
        if(timer.owner!=owner) timer={owner,0};
        if(b->IsUnderEMP() || !b->IsPowerOnline() || b->BeingWarpedOut) continue;
        if(++timer.elapsed>=rules.interval) {
            Research::award(owner,rules.points); timer.elapsed=0;
        }
    }
}
}

void tickBuildings() {
    if(!Research::isEnabled()) return;
    tickGlobalResearch();
    if(needsBuildingScan()) tickConfiguredBuildings();
    const unsigned frame=static_cast<unsigned>(Unsorted::CurrentFrame);
    if(Research::allowAIResearch && frame%15==0) {
        for(auto* house:HouseClass::Array) {
            if(!house || !house->Type || !eligibleHouse(house) || house->IsControlledByHuman()) continue;
            int option=-1;
            for(const auto& [id,ticket]:Research::freeTickets) {
                if(ticket.house!=house->ArrayIndex) continue;
                auto rows=Research::ticketOptions(id,ticket.house,house->Type->ID);
                for(int i:rows) if(Research::claimFree(Research::ticketFlag|id,house->ArrayIndex,i)) {option=i;break;}
                if(option>=0) break;
            }
            if(option<0) option=Research::purchaseForAI(house->ArrayIndex,house->Type->ID,frame);
            if(option>=0) Research::Detail::showResearchSelection(house,option);
        }
    }
}
void authorizeBuildingReward(InfantryClass* infantry,BuildingClass* building,Research::BuildingRewardSource source) {
    if(source==Research::BuildingRewardSource::None) return;
    if(Research::isEnabled() && infantry && eligibleHouse(infantry->Owner) && building && building->Owner && building->Type && alive(building)) {
        const auto& rule=rewardRule(building->Type);
        if(rule.options.empty()) return;
        if(source==Research::BuildingRewardSource::Engineer && (!rule.engineer || !infantry->Type || !infantry->Type->Engineer || building->Owner!=infantry->Owner)) return;
        if(source==Research::BuildingRewardSource::Spy && !rule.spy) return;
        auto [it,added]=Research::buildingRewards.try_emplace(building->UniqueID,Research::BuildingReward{building->Owner->ArrayIndex,-1,false});
        auto& reward=it->second;
        if(reward.claimed) return;
        const int house=infantry->Owner->ArrayIndex;
        const bool newlyGranted=reward.owner!=building->Owner->ArrayIndex || reward.authorized!=house;
        reward.owner=building->Owner->ArrayIndex;reward.authorized=house;reward.source=source;
        if(newlyGranted && !Research::buildingOptions(building->UniqueID,house).empty())
            Research::Detail::notifyFreeResearch(house);
    }
}
} // namespace BuildingResearch

std::vector<int> Research::buildingOptions(unsigned long id,int house) {
    std::vector<int> result;
    if(!isEnabled()) return result;
    auto state=buildingRewards.find(id);
    if(state==buildingRewards.end() || state->second.claimed || state->second.authorized!=house) return result;
    BuildingClass* building=nullptr;
    for(auto* b:BuildingClass::Array) if(b && b->UniqueID==id) {building=b;break;}
    if(!building || !building->Type || !alive(building) || !building->IsOnMap || building->InLimbo || !building->Owner || building->Owner->ArrayIndex!=state->second.owner) return result;
    const auto& rule=BuildingResearch::rewardRule(building->Type);
    if((state->second.source==BuildingRewardSource::Engineer && !rule.engineer) ||
       (state->second.source==BuildingRewardSource::Spy && !rule.spy) ||
       state->second.source==BuildingRewardSource::None) return result;
    return Detail::buildingOfferRows(id,house,rule.options);
}
std::vector<unsigned long> Research::pendingBuildingRewards(int house) {
    std::vector<unsigned long> result;
    for(const auto& [id,reward]:buildingRewards)
        if(reward.authorized==house && !reward.claimed && !buildingOptions(id,house).empty()) result.push_back(id);
    return result;
}
bool Research::claimBuilding(unsigned long id,int house,int option) {
    return claimFree(id,house,option);
}
std::vector<Research::RewardId> Research::pendingFreeRewards(int house) {
    const auto buildings=pendingBuildingRewards(house);
    std::vector<RewardId> result(buildings.begin(),buildings.end());
    if(house<0 || house>=HouseClass::Array.Count) return result;
    const auto h=HouseClass::Array.Items[house];
    if(h && h->Type) for(const auto& [id,ticket]:freeTickets)
        if(ticket.house==house && !ticketOptions(id,house,h->Type->ID).empty()) result.push_back(ticketFlag|id);
    return result;
}
std::vector<int> Research::freeOptions(RewardId id,int house) {
    if(!(id&ticketFlag)) return buildingOptions(static_cast<unsigned long>(id),house);
    if(house<0 || house>=HouseClass::Array.Count) return {};
    const auto h=HouseClass::Array.Items[house];
    return h && h->Type?ticketOptions(static_cast<unsigned long>(id),house,h->Type->ID):vector<int>{};
}
bool Research::claimFree(RewardId id,int house,int option) {
    const auto board=freeOptions(id,house);
    if(std::find(board.begin(),board.end(),option)==board.end() || !executeEffect(house,option)) return false;
    const auto serial=static_cast<unsigned long>(id);
    if(id&ticketFlag) freeTickets.erase(serial);
    else {
        auto& reward=buildingRewards.at(serial);
        reward.claimed=true;reward.authorized=-1;reward.source=BuildingRewardSource::None;
        refreshedSlots.erase(house);
    }
    Detail::recordAcquisition(house,option);
    const auto country=HouseClass::Array.Items[house]->Type->ID;
    savedOffers.erase(house);seenOffers.erase(house);clearOfferCache();
    if(id&ticketFlag) offers(house,country);
    grantProjectRewards(house,option,country);
    return true;
}

DEFINE_HOOK(0x519F84, SF_EngineerResearchBuilding, 6) {
    BuildingResearch::authorizeBuildingReward(R->ESI<InfantryClass*>(),R->EDI<BuildingClass*>(),Research::BuildingRewardSource::Engineer);
    return 0;
}
DEFINE_HOOK(0x51A002, SF_SpyResearchBuilding, 6) {
    BuildingResearch::authorizeBuildingReward(R->ESI<InfantryClass*>(),R->EDI<BuildingClass*>(),Research::BuildingRewardSource::Spy);
    return 0;
}
