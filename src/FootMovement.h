#pragma once

#include <FootClass.h>
#include <LocomotionClass.h>
#include <MapClass.h>

namespace FootMovement {
inline void stopOrders(FootClass* foot) {
    foot->ClearMegaMissionData();
    foot->NavQueue.Count=0;
    foot->SetTarget(nullptr);
    foot->SetDestination(nullptr,true);
    foot->StopMoving();
    foot->ForceMission(Mission::Sleep);
}
// Stop_Moving lets a drive already on a turn track finish that track. A moving
// target needs a stationary instance of the same locomotor at its current XY.
inline bool interruptTravel(FootClass* foot) {
    auto old=foot->Locomotor.GetInterfacePtr();
    if(!old->Is_Moving() && !old->Is_Moving_Now()) {stopOrders(foot);return true;}
    IPiggyback* piggy=nullptr;
    if(SUCCEEDED(old->QueryInterface(__uuidof(IPiggyback),reinterpret_cast<void**>(&piggy)))) {
        bool borrowed=piggy->Is_Piggybacking();piggy->Release();
        if(borrowed) return false;
    }
    IPersistStream* persist=nullptr;
    if(FAILED(old->QueryInterface(__uuidof(IPersistStream),reinterpret_cast<void**>(&persist)))) return false;
    CLSID clsid{};
    auto hr=persist->GetClassID(&clsid);persist->Release();
    if(FAILED(hr)) return false;
    ILocomotion* replacement=nullptr;
    if(FAILED(LocomotionClass::CreateInstance(&replacement,&clsid,nullptr,CLSCTX_INPROC_SERVER)) || !replacement) return false;
    if(FAILED(replacement->Link_To_Object(foot))) {replacement->Release();return false;}
    bool powered=old->Is_Powered();
    stopOrders(foot);
    old->Mark_All_Occupation_Bits(MarkType::Up);
    foot->Mark(MarkType::Up);
    foot->Locomotor.Attach(replacement);
    if(!powered) replacement->Power_Off();
    replacement->Unlimbo();
    foot->Mark(MarkType::Down);
    return true;
}
// SetLocation/Mark move the object and its cell chain, but FootClass normally
// updates LastMapCoords, threat regions, sensors and neighbouring-cell counts
// only at PCPType::End. Forced motion bypasses that locomotor path. Move these
// registrations together without running cell-entry actions (crushing, mines,
// building entry, etc.) while IgnoreCollision units are still in transit.
inline void syncCellRegistration(FootClass* foot) {
    const auto previous=foot->LastMapCoords,current=foot->GetMapCoords();
    if(previous==current || previous==CellStruct{0,0} || !foot->Owner) return;
    auto& map=MapClass::Instance;
    auto oldCell=map.TryGetCellAt(previous),newCell=map.TryGetCellAt(current);
    if(!oldCell || !newCell) return;
    if(foot->GetTechnoType()->SensorsSight) {
        foot->RemoveSensorsAt(previous);
        foot->AddSensorsAt(current);
    }
    const auto house=foot->GetOwningHouseIndex();
    oldCell->UpdateThreat(house,-static_cast<int>(foot->ThreatPosed));
    foot->ThreatPosed=0;
    foot->ThreatPosed=foot->GetThreatValue();
    newCell->UpdateThreat(house,static_cast<int>(foot->ThreatPosed));
    for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x) {
        if(!x && !y) continue;
        if(auto cell=map.TryGetCellAt(CellStruct{static_cast<short>(previous.X+x),static_cast<short>(previous.Y+y)}))
            --cell->BlockedNeighbours;
        if(auto cell=map.TryGetCellAt(CellStruct{static_cast<short>(current.X+x),static_cast<short>(current.Y+y)}))
            ++cell->BlockedNeighbours;
    }
    foot->LastMapCoords=current;
}
}
