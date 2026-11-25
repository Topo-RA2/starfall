#include <BuildingClass.h>
#include <CCINIClass.h>
#include <EventClass.h>
#include <HouseClass.h>
#include <InfantryClass.h>
#include <MapClass.h>
#include <Unsorted.h>
#include <BulletTypeClass.h>
#include <WeaponTypeClass.h>
#include <WWKeyboardClass.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <tuple>
#include <vector>
#include "ActSp.h"
#include "BetterGarrison.h"
#include "HighPlatforms.h"

namespace BetterGarrison {
std::map<DWORD,Order> orders;

namespace {
struct Rules {
    bool enabled;
    double damage;
    int range;
    double rate;
};
std::optional<Rules> rules;
std::map<const BuildingTypeClass*,Rules> buildings;
std::map<DWORD,TargetClass> occupied;

Rules readRules(const char* section,const Rules& fallback) {
    auto ini=CCINIClass::INI_Rules;
    Rules result{
        ini->ReadBool(section,"BetterGarrison.Enabled",fallback.enabled),
        ini->ReadDouble(section,"BetterGarrison.DamageMultiplier",fallback.damage),
        ini->ReadInteger(section,"BetterGarrison.RangeBonus",fallback.range),
        ini->ReadDouble(section,"BetterGarrison.FireRateMultiplier",fallback.rate)
    };
    if(!std::isfinite(result.damage) || result.damage<0 || result.damage>100) result.damage=fallback.damage;
    result.range=std::clamp(result.range,-128,128);
    if(!std::isfinite(result.rate) || result.rate<0.01 || result.rate>100) result.rate=fallback.rate;
    return result;
}

const Rules& settings() {
    if(!rules) rules=readRules("General",{false,1.20,1,1.0});
    return *rules;
}

const Rules& buildingSettings(const BuildingTypeClass* type) {
    auto [it,added]=buildings.try_emplace(type);
    if(added) it->second=readRules(type->ID,settings());
    return it->second;
}

bool enabled(const BuildingClass* post) {
    if(!CCINIClass::INI_Rules || !settings().enabled || !post->Type->CanBeOccupied ||
       !post->Type->CanOccupyFire || post->Type->MaxNumberOccupants<=0) return false;
    return buildingSettings(post->Type).enabled;
}

bool ready(const BuildingClass* post) {
    return post->Owner && !post->Owner->Defeated && post->IsAlive && post->Health>0 && post->IsOnMap &&
           !post->InLimbo && !post->Deactivated && !post->IsUnderEMP() && !post->IsBeingWarpedOut() &&
           post->IsPowerOnline() && post->CurrentMission!=Mission::Selling && post->CurrentMission!=Mission::Construction;
}

bool resident(const BuildingClass* post,const InfantryClass* unit) {
    return unit && unit->IsAlive && unit->Health>0 && unit->InLimbo &&
           unit->Owner==post->Owner && !unit->Transporter;
}

bool interactive(const BuildingClass* post) {
    return post && post->Occupants.Count && enabled(post) && ready(post) && post->Owner->IsControlledByCurrentPlayer();
}

CellClass* playableCell(CellStruct cell) {
    auto& map=MapClass::Instance;
    return cell.X>=0 && cell.X<512 && cell.Y>=0 && cell.Y<512 && map.IsWithinUsableArea(cell,false)
        ? map.TryGetCellAt(cell) : nullptr;
}

AbstractClass* resolveTarget(TargetClass target) {
    if(target.m_RTTI==static_cast<unsigned char>(AbstractType::Cell)) {
        if(target.m_ID<0 || target.m_ID>=512000 || target.m_ID%1000>=512) return nullptr;
        return playableCell({static_cast<short>(target.m_ID%1000),static_cast<short>(target.m_ID/1000)});
    }
    return target.As_Techno();
}

struct FiringPosition {
    InfantryClass* unit;
    CoordStruct port;
    const Rules& rules;
};
const FiringPosition* firing=nullptr;
const BuildingClass* readingPort=nullptr;
bool committed=false;
int evaluatedSlot=-1;

CoordStruct firingPort(BuildingClass* post,int index) {
    static const bool extendedPorts=GetModuleHandleW(L"Phobos.dll")!=nullptr;
    if(index>=10 && !extendedPorts) return post->GetCoords();
    const auto previous=readingPort;
    const int oldIndex=post->FiringOccupantIndex;
    readingPort=post;
    post->FiringOccupantIndex=index;
    auto result=post->GetFLH(0,post->GetCoords());
    post->FiringOccupantIndex=oldIndex;
    readingPort=previous;
    return result;
}

class ScopedFiringPosition {
public:
    ScopedFiringPosition(BuildingClass* post,InfantryClass* unit,int index)
        : reference(unit),savedLocation(unit->Location),
          position{unit,firingPort(post,index),buildingSettings(post->Type)},previous(firing) {
        // Art muzzle flashes can project several cells away from the building's footprint.
        // Use the building's center for range and keep the muzzle for projectile launch.
        unit->Location=post->GetCenterCoords();
        firing=&position;
    }

    ~ScopedFiringPosition() {
        firing=previous;
        if(auto unit=survivor();unit && unit->InLimbo) unit->Location=savedLocation;
    }

    InfantryClass* survivor() const {
        auto target=reference;
        return target.As_Infantry();
    }

    double fireRate() const { return position.rules.rate; }

private:
    TargetClass reference;
    CoordStruct savedLocation;
    FiringPosition position;
    const FiringPosition* previous;
};

bool legalTarget(const InfantryClass* unit,AbstractClass* aim) {
    if(aim && aim->WhatAmI()==AbstractType::Cell) return true;
    const auto target=abstract_cast<TechnoClass*>(aim);
    if(!target || !target->Owner || unit->Owner->IsAlliedWith(target->Owner) ||
       !target->IsAlive || target->Health<=0 || !target->IsOnMap || target->InLimbo ||
       !target->GetTechnoType()->LegalTarget || target->GetTechnoType()->Invisible ||
       (target->Disguised && !target->IsClearlyVisibleTo(unit->Owner))) return false;
    if(target->CloakState==CloakState::Uncloaked) return true;
    const auto cell=target->GetCell();
    const int house=unit->Owner->ArrayIndex;
    return cell && house>=0 && house<24 && cell->Sensors_InclHouse(house);
}

bool automaticTarget(const TechnoClass* target) {
    return target && target->Owner && !target->Owner->IsNeutral() && !target->Owner->IsObserver() &&
           !target->GetTechnoType()->Insignificant;
}

int weaponSlot(InfantryClass* unit,AbstractClass* target) {
    int slot=unit->Type->OpenTransportWeapon;
    if(slot==-1) slot=unit->SelectWeapon(target);
    auto mount=slot>=0 && (slot<2 || slot<unit->Type->WeaponCount)?unit->GetWeapon(slot):nullptr;
    auto weapon=mount?mount->WeaponType:nullptr;
    if(!weapon || !weapon->Projectile || !weapon->Warhead || !weapon->FireInTransport || weapon->NeverUse) return -1;
    if(!(target->IsInAir()?weapon->Projectile->AA:weapon->Projectile->AG)) return -1;
    return slot;
}

bool canFireAt(InfantryClass* unit,AbstractClass* target,int slot) {
    const auto error=unit->GetFireError(target,slot,true);
    return error==FireError::OK || error==FireError::FACING || error==FireError::ROTATING || error==FireError::REARM;
}

int commandSlot(InfantryClass* first,AbstractClass* target) {
    if(!legalTarget(first,target)) return -1;
    const int slot=weaponSlot(first,target);
    if(slot<0) return -1;
    // Query the normal weapon checks without the infantry's animation state or cooldown.
    using GetWeaponError=FireError (__thiscall*)(TechnoClass*,AbstractClass*,int,bool);
    const auto getWeaponError=reinterpret_cast<GetWeaponError>(0x6FC0B0);
    const auto timer=first->RearmTimer;
    first->RearmTimer.Stop();
    const auto error=getWeaponError(first,target,slot,true);
    first->RearmTimer=timer;
    const bool allowed=error==FireError::OK || error==FireError::FACING || error==FireError::ROTATING ||
        error==FireError::REARM || error==FireError::RANGE;
    return allowed?slot:-1;
}

int inRange(InfantryClass* unit,AbstractClass* target) {
    if(!legalTarget(unit,target)) return -1;
    const int slot=weaponSlot(unit,target);
    return slot>=0 && unit->IsCloseEnough(target,slot) && canFireAt(unit,target,slot)?slot:-1;
}

bool operational(const InfantryClass* unit) {
    return !unit->Deactivated && !unit->IsUnderEMP() && !unit->IsBeingWarpedOut();
}

FireError checkCommand(BuildingClass* post,AbstractClass* target,bool checkRange) {
    if(!target || !post->Occupants.Count) return FireError::ILLEGAL;
    // The first occupant's weapon determines which targets can be ordered.
    auto first=post->Occupants[0];
    if(!resident(post,first)) return FireError::ILLEGAL;
    {
        const ScopedFiringPosition position(post,first,0);
        const int slot=commandSlot(first,target);
        if(slot<0) return FireError::ILLEGAL;
        if(!checkRange || (operational(first) && first->IsCloseEnough(target,slot) && canFireAt(first,target,slot)))
            return FireError::OK;
    }
    for(int i=1;i<post->Occupants.Count;++i) {
        auto unit=post->Occupants[i];
        if(!resident(post,unit) || !operational(unit)) continue;
        const ScopedFiringPosition position(post,unit,i);
        if(inRange(unit,target)>=0) return FireError::OK;
    }
    return FireError::RANGE;
}

bool evaluate(InfantryClass* unit,TechnoClass* target,int slot,int& score) {
    constexpr int targetKinds=(1 << int(AbstractType::Aircraft)) | (1 << int(AbstractType::Building)) |
        (1 << int(AbstractType::Infantry)) | (1 << int(AbstractType::Unit));
    auto scratch=CoordStruct::Empty;
    const int previous=evaluatedSlot;
    evaluatedSlot=slot;
    // InRange already applied the exact weapon range, including the garrison bonus.
    const bool allowed=unit->CanAutoTargetObject(ThreatType::Range,targetKinds,-1,target,&score,UINT_MAX,&scratch);
    evaluatedSlot=previous;
    return allowed;
}

std::pair<TechnoClass*,int> acquire(InfantryClass* unit) {
    TechnoClass* best=nullptr;
    int bestSlot=-1,bestScore=INT_MIN;
    for(auto target:TechnoClass::Array) {
        if(!automaticTarget(target)) continue;
        const int slot=inRange(unit,target);
        int score=0;
        if(slot<0 || !evaluate(unit,target,slot,score)) continue;
        if(score<bestScore || (score==bestScore && best && target->UniqueID>=best->UniqueID)) continue;
        best=target;
        bestSlot=slot;
        bestScore=score;
    }
    return {best,bestSlot};
}

AbstractClass* commanded(BuildingClass* post) {
    auto found=orders.find(post->UniqueID);
    if(found==orders.end()) return nullptr;
    TargetClass target;
    target.m_RTTI=static_cast<unsigned char>(found->second.kind);
    target.m_ID=found->second.target;
    auto aim=found->second.house==post->Owner->ArrayIndex?resolveTarget(target):nullptr;
    if(checkCommand(post,aim,false)!=FireError::OK) {
        orders.erase(found);
        return nullptr;
    }
    return aim;
}

void fire(BuildingClass* post,InfantryClass* unit,int index,AbstractClass* ordered) {
    if(unit->RearmTimer.GetTimeLeft()>0 || !operational(unit)) return;
    const bool search=unit->Type->CanPassiveAquire && (static_cast<DWORD>(Unsorted::CurrentFrame)+unit->UniqueID)%8==0;
    if(!ordered && !unit->Target && !search) return;
    const ScopedFiringPosition position(post,unit,index);
    auto aim=ordered;
    int slot=inRange(unit,aim);
    if(slot<0) {
        const auto previous=abstract_cast<TechnoClass*>(unit->Target);
        const auto retained=unit->Type->CanPassiveAquire && automaticTarget(previous)?previous:nullptr;
        aim=retained;
        slot=inRange(unit,retained);
        int score=0;
        if(slot>=0 && !evaluate(unit,retained,slot,score)) slot=-1;
    }
    if(slot<0 && search) {
        std::tie(aim,slot)=acquire(unit);
    }
    if(slot>=0) {
        unit->SetTarget(aim);
        const auto facing=unit->GetTargetDirection(aim);
        unit->PrimaryFacing.SetCurrent(facing);
        unit->SecondaryFacing.SetCurrent(facing);
        if(unit->GetFireError(aim,slot,true)==FireError::OK) {
            committed=false;
            unit->Fire(aim,slot);
            if(auto survivor=position.survivor();survivor && committed && survivor->CurrentBurstIndex==0) {
                const int delay=survivor->RearmTimer.GetTimeLeft();
                if(delay>0 && position.fireRate()!=1.0) {
                    const auto scaled=std::ceil(delay/position.fireRate());
                    survivor->RearmTimer.Start(static_cast<int>(std::clamp(scaled,1.0,double(INT_MAX))));
                }
            }
        }
    } else if(unit->Target) unit->SetTarget(nullptr);
}
}

void reset() {
    buildings.clear();
    occupied.clear();
    orders.clear();
    rules.reset();
}

void tick() {
    if(!CCINIClass::INI_Rules || !settings().enabled) return;
    occupied.clear();
    // Firing a scripted or instant weapon can invalidate a building or occupant.
    std::vector<TargetClass> active;
    for(auto post:BuildingClass::Array) {
        if(!post || !enabled(post) || !post->IsAlive) continue;
        if(!post->Occupants.Count) {
            orders.erase(post->UniqueID);
            continue;
        }
        const TargetClass reference(post);
        for(auto unit:post->Occupants) if(resident(post,unit)) occupied.emplace(unit->UniqueID,reference);
        if(ready(post)) active.push_back(reference);
    }
    for(auto reference:active) {
        auto post=reference.As_Building();
        if(!post || !ready(post)) continue;
        std::vector<TargetClass> garrison;
        for(auto unit:post->Occupants) garrison.emplace_back(unit);
        for(auto occupant:garrison) {
            post=reference.As_Building();
            if(!post || !ready(post)) break;
            auto unit=occupant.As_Infantry();
            if(!resident(post,unit)) continue;
            for(int index=0;index<post->Occupants.Count;++index) if(post->Occupants[index]==unit) {
                fire(post,unit,index,commanded(post));
                break;
            }
        }
    }
}
}

using namespace BetterGarrison;

bool BetterGarrison::isOccupant(TechnoClass* unit) {
    if(!unit || !unit->InLimbo || unit->WhatAmI()!=AbstractType::Infantry) return false;
    auto found=occupied.find(unit->UniqueID);
    if(found==occupied.end()) return false;
    auto post=found->second.As_Building();
    if(!post || !post->IsAlive || !post->IsOnMap) return false;
    auto infantry=static_cast<InfantryClass*>(unit);
    if(!resident(post,infantry)) return false;
    for(auto occupant:post->Occupants) if(occupant==infantry) return true;
    return false;
}

// Disable the virtual test for legacy garrison fire; entry, exit and pips use the occupant list.
DEFINE_HOOK(0x458DD0,ActSp_DisableLegacyGarrisonFire,6) {
    return enabled(R->ECX<BuildingClass*>())?0x458DFC:0;
}

// Ordinary building weapons use their own FLH. The explicit port query keeps the native offsets.
DEFINE_HOOK(0x453849,ActSp_GarrisonBuildingPort,6) {
    auto post=R->ESI<BuildingClass*>();
    return post!=readingPort && enabled(post)?0x4538C5:0;
}

DEFINE_HOOK(0x523259,ActSp_GarrisonInfantryPort,6) {
    if(firing && firing->unit==R->ESI<InfantryClass*>()) {
        auto out=R->Stack<CoordStruct*>(0x20);
        *out=firing->port;
        R->EAX(out);
        return 0x5232CD;
    }
    return 0;
}

// Native garrison entry leaves the infantry walking sequence and destination in limbo.
// The generic Techno fire checks have already completed at this point.
DEFINE_HOOK(0x51C9B8,ActSp_GarrisonStationaryFire,6) {
    return firing && firing->unit==R->EBX<InfantryClass*>()?0x51CACD:0;
}

// EvaluateObject must inspect the same weapon the occupant will fire. The native
// selector has already run here and the engine has consumed its target argument.
DEFINE_HOOK(0x6F7CC4,ActSp_GarrisonEvaluateWeapon,5) {
    if(evaluatedSlot>=0 && firing && firing->unit==R->EDI<InfantryClass*>()) R->EAX(evaluatedSlot);
    return 0;
}

// Apply the bonus after native and Starfall weapon range modifiers.
DEFINE_HOOK(0x6F72E9,ActSp_GarrisonRange,6) {
    if(firing && firing->unit==R->ESI<InfantryClass*>()) {
        const auto range=static_cast<long long>(R->EDI<int>())+firing->rules.range*256;
        R->EDI(static_cast<int>(std::clamp(range,0LL,static_cast<long long>(INT_MAX))));
    }
    return 0;
}

DEFINE_HOOK(0x6FE460,ActSp_GarrisonDamage,6) {
    if(firing && firing->unit==R->ESI<InfantryClass*>() && firing->rules.damage!=1.0) {
        const auto damage=R->Stack<int>(0x2C)*firing->rules.damage;
        const auto scaled=static_cast<int>(std::clamp(damage,double(INT_MIN),double(INT_MAX)));
        R->EDI(scaled);
        R->Stack(0x2C,scaled);
    }
    return 0;
}

DEFINE_HOOK(0x6FF66C,ActSp_GarrisonShotCommitted,6) {
    if(firing && firing->unit==R->ESI<InfantryClass*>()) committed=true;
    return 0;
}

DEFINE_HOOK(0x447218,ActSp_GarrisonAttackCursor,6) {
    auto post=R->ESI<BuildingClass*>();
    if(!interactive(post)) return 0;
    auto target=abstract_cast<TechnoClass*>(R->Stack<AbstractClass*>(0x1C));
    const auto error=checkCommand(post,target,true);
    if(error==FireError::ILLEGAL) return 0;
    R->EAX(error==FireError::OK?Action::Attack:Action::NoMove);
    return 0x447273;
}

DEFINE_HOOK(0x447548,ActSp_GarrisonGroundAction,6) {
    auto post=R->ESI<BuildingClass*>();
    const auto cell=R->Stack<const CellStruct*>(0x1C);
    if(!cell || R->Stack<bool>(0x24) || !WWKeyboardClass::Instance->IsForceFireKeyPressed() || !interactive(post)) return 0;
    R->EBX(checkCommand(post,playableCell(*cell),true)==FireError::OK?Action::Attack:Action::NoMove);
    return 0x44776D;
}

// Choose the stationary pointer before the engine or Ares tests the building's own weapon range.
DEFINE_HOOK(0x4AB34A,ActSp_GarrisonAttackPointer,8) {
    if(R->Stack<Action>(0x10)!=Action::Attack) return 0;
    const auto& selected=ObjectClass::CurrentObjects;
    if(!selected.Count) return 0;
    for(auto object:selected) if(!interactive(abstract_cast<BuildingClass*>(object))) return 0;
    return 0x4AB525;
}

void BetterGarrison::applyOrder(TechnoClass* unit,EventClass* event) {
    auto post=abstract_cast<BuildingClass*>(unit);
    if(!post || !post->Occupants.Count || !post->Owner || !enabled(post)) return;
    if(event->MegaMission.Mission!=static_cast<BYTE>(Mission::Attack)) orders.erase(post->UniqueID);
    else if(auto target=resolveTarget(event->MegaMission.Target); checkCommand(post,target,false)==FireError::OK) {
        const auto reference=TargetClass(target);
        orders[post->UniqueID]={post->Owner->ArrayIndex,reference.m_ID,static_cast<AbstractType>(reference.m_RTTI)};
    }
}

void ActSp::stopOrder(TechnoClass* object) {
    if(object) {
        orders.erase(object->UniqueID);
        HighPlatforms::forgetEntry(object->UniqueID);
    }
}
