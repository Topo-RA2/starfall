#pragma once
#include "Research.h"
#include <functional>

class TechnoClass;
class TechnoTypeClass;
class HouseClass;

// Internal research interfaces. Runtime bridges are implemented beside the passive
// registry and conversion engine; research never accesses their private containers.
namespace Research::Detail {
extern std::array<std::wstring,rarityCount> rarityNameOverrides;
using ResearchWarning=std::function<void(const std::string&,const char*,const std::string&)>;
using EffectWarning=std::function<void(const char*,const std::string&)>;
inline EffectSpec singleEffect(const Option& o) {
    return {o.effect,o.passive,o.technology,o.from,o.to,o.types,o.credits,o.cost,o.reward};
}
bool validateEffect(const EffectSpec& effect,const EffectWarning& warn={});
bool hasProductionLicense(const HouseClass* house,TechnoTypeClass* type);
void installProductionCheck();
void clearEffectCaches();
unsigned matchSeed();
int randomIndex(int count);
const std::vector<int>& buildingOfferRows(unsigned long id,int house,const std::vector<std::string>& configured);
void notifyFreeResearch(int house);
void showResearchSelection(HouseClass* house,int option);
void announceReward(int house,int option);

namespace Runtime {
bool hasPassive(const std::string& id);
bool eligibleHouse(HouseClass* house);
bool conversionAvailable();
bool nativeConversionTypesSafe(const TechnoTypeClass* from,const TechnoTypeClass* to);
bool canTransformUnit(TechnoClass* unit,TechnoTypeClass* to);
bool transformUnit(TechnoClass* unit,TechnoTypeClass* to);
}
}
