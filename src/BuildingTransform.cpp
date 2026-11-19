#include "BuildingTransform.h"
#include "BuildingSpawn.h"
#include "UnitHealth.h"
#include <BuildingClass.h>
#include <HouseClass.h>
#include <MapClass.h>
#include <algorithm>
#include <vector>

namespace BuildingTransform {
namespace {
using Cells=std::vector<std::pair<short,short>>;
Cells foundation(const BuildingTypeClass* type,bool bib) {
    Cells result;
    if(auto cells=type->GetFoundationData(bib))
        for(;*cells!=CellStruct{0x7FFF,0x7FFF};++cells) result.emplace_back(cells->X,cells->Y);
    std::sort(result.begin(),result.end());
    result.erase(std::unique(result.begin(),result.end()),result.end());
    return result;
}
}
bool sameFoundation(const BuildingTypeClass* from,const BuildingTypeClass* to) {
    if(!from || !to) return false;
    // Vanilla YR ignores GetFoundationData's IncludeBib argument. Explicitly
    // match bib bounds too; extensions may additionally return distinct cells.
    if(from->Bib!=to->Bib || (from->Bib &&
       (from->GetFoundationWidth()!=to->GetFoundationWidth() ||
        from->GetFoundationHeight(true)!=to->GetFoundationHeight(true)))) return false;
    // Compare the actual occupied cells and the bib separately. Matching only
    // dimensions would incorrectly accept rotated / hollow custom foundations.
    for(bool bib:{false,true}) {
        const auto a=foundation(from,bib),b=foundation(to,bib);
        if(a.empty() || a!=b) return false;
    }
    return true;
}
bool eligible(BuildingClass* b,BuildingTypeClass* to) {
    if(!b || !b->IsAlive || b->Health<=0 || !b->IsOnMap || b->InLimbo || !b->Owner ||
       b->Owner->Defeated || b->Owner->IsObserver() || !BuildingSpawn::supported(b->Type) ||
       !BuildingSpawn::supported(to) || b->Type==to || b->Type->Strength<=0 || to->Strength<=0 ||
       !sameFoundation(b->Type,to)) return false;
    // Live production, occupants and native multi-object controllers cannot be
    // handed to a different building constructor without losing their links.
    if(b->Factory || b->Passengers.NumPassengers || b->Occupants.Count || b->HasAnyLink() ||
       b->BunkerLinkedItem || b->SpawnManager || b->SlaveManager || b->CaptureManager ||
       b->IsMindControlled() || b->MindControlledByHouse || b->TemporalTargetingMe ||
       b->TemporalImUsing || b->Airstrike || b->DrainingMe || b->DrainTarget || b->AttachedTag ||
       b->C4Applied || b->Overpowerers.Count || b->BeingWarpedOut || b->WarpingOut ||
       b->IsImmobilized || b->IsBeingManipulated || b->CurrentMission==Mission::Construction ||
       b->CurrentMission==Mission::Selling) return false;
    for(auto upgrade:b->Upgrades) if(upgrade) return false;
    return true;
}
BuildingClass* convert(BuildingClass* old,BuildingTypeClass* to,Rebind rebind) {
    if(!eligible(old,to)) return nullptr;
    auto owner=old->Owner;
    const auto location=old->Location;
    auto anchor=old->GetMapCoords();
    const auto hp=UnitHealth::read(old);
    const int maximum=static_cast<int>(std::clamp<int64_t>(int64_t(to->Strength)+hp.maximum-old->Type->Strength,1,INT_MAX));
    const int health=static_cast<int>(std::clamp<int64_t>((int64_t(hp.current)*maximum+hp.maximum/2)/hp.maximum,1,maximum));
    const bool selected=old->IsSelected;
    const auto mission=old->CurrentMission,suspended=old->SuspendedMission,queued=old->QueuedMission;
    const int status=old->MissionStatus,start=old->CurrentMissionStartTime,elapsed=old->MissionAccumulateTime;
    const int bstate=old->BState,queuedBstate=old->QueueBState;
    const auto updateTimer=old->UpdateTimer;
    const bool ready=old->IsReadyToCommence;
    auto current=static_cast<BuildingClass*>(to->CreateObject(owner));
    if(!current) return nullptr;
    auto& index=AbstractClass::TargetIndex;
    const int oldKey=old->Fetch_ID(),newKey=current->Fetch_ID();
    if(oldKey!=static_cast<int>(old->UniqueID) || newKey!=static_cast<int>(current->UniqueID) ||
       !index.IsPresent(oldKey) || !index.IsPresent(newKey)) {
        current->UnInit();return nullptr;
    }
    current->QueueMission(Mission::Construction,false);
    current->NextMission();
    if(!old->Limbo()) {current->UnInit();return nullptr;}
    // The old building still exists until placement succeeds, allowing rollback.
    if(!to->CanPlaceHere(&anchor,owner) || !current->Unlimbo(location,DirType::North)) {
        current->UnInit();
        if(!old->Unlimbo(location,DirType::North))
            OutputDebugStringA("Starfall: building conversion rollback could not restore placement\n");
        else {
            // Unlimbo starts construction even for an already completed object.
            // Restore its activity without Place(), which would grant free units
            // and other construction rewards a second time.
            old->BeginMode(static_cast<BStateType>(bstate));
            old->CurrentMission=mission;old->SuspendedMission=suspended;old->QueuedMission=queued;
            old->MissionStatus=status;old->CurrentMissionStartTime=start;old->MissionAccumulateTime=elapsed;
            old->UpdateTimer=updateTimer;old->QueueBState=queuedBstate;old->IsReadyToCommence=ready;
        }
        if(selected) old->Select();
        return nullptr;
    }
    current->Veterancy=old->Veterancy;
    current->CurrentRanking=old->CurrentRanking;
    current->FirepowerMultiplier=old->FirepowerMultiplier;
    current->ArmorMultiplier=old->ArmorMultiplier;
    current->Ammo=to->Ammo<0?-1:old->Ammo<0?to->Ammo:std::min(old->Ammo,to->Ammo);
    current->ReloadTimer=old->ReloadTimer;
    current->RearmTimer=old->RearmTimer;
    current->IronCurtainTimer=old->IronCurtainTimer;
    current->IronTintTimer=old->IronTintTimer;
    current->IronTintStage=old->IronTintStage;
    current->ForceShielded=old->ForceShielded;
    current->Cloakable=old->Cloakable || current->Cloakable;
    current->CloakState=old->CloakState;
    current->CloakProgress=old->CloakProgress;
    current->CloakDelayTimer=old->CloakDelayTimer;
    current->Berzerk=old->Berzerk;
    current->BerzerkDurationLeft=old->BerzerkDurationLeft;
    current->EMPLockRemaining=old->EMPLockRemaining;
    current->Deactivated=old->Deactivated;
    current->Group=old->Group;
    current->HasBeenCaptured=old->HasBeenCaptured;
    current->HasEngineer=old->HasEngineer;
    current->ShouldRebuild=old->ShouldRebuild;
    current->AI_Sellable=old->AI_Sellable;
    current->BeginMode(BStateType::Idle);
    current->QueueMission(Mission::Guard,false);
    current->NextMission();
    current->Place(false);
    // Preserve the public object identity, leaving the allocation counter alone.
    // No engine calls occur between these assignments. The retiring object now
    // owns the fresh, unused ID, so its cleanup cannot erase persistent state.
    // Entries hold native interface pointers; swap their values rather than
    // constructing a pointer with an assumed multiple-inheritance offset.
    std::swap(index.FetchIndex(oldKey),index.FetchIndex(newKey));
    std::swap(old->UniqueID,current->UniqueID);
    if(maximum==to->Strength) {
        UnitHealth::pools.erase(current->UniqueID);
        current->Health=current->EstimatedHealth=health;
    } else {
        auto& pool=UnitHealth::pools[current->UniqueID];
        pool.base=to->Strength;pool.maximum=maximum;pool.set(current,health);
    }
    if(rebind) rebind(old,current);
    old->UnInit();
    owner->RecheckPower=true;
    owner->RecheckTechTree=true;
    current->MarkForRedraw();
    if(selected) current->Select();
    return current;
}
}
