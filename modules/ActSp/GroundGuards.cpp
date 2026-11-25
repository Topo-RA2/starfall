#include <HouseClass.h>
#include <MapClass.h>
#include <TargetClass.h>
#include <Unsorted.h>
#include "GroundGuards.h"
#include "GuardPoints.h"
#include "BetterGarrison.h"

namespace GroundGuards {
std::map<DWORD,Slot> slots;

namespace {
int distanceSquared(CellStruct a,CellStruct b) {
    const int dx=a.X-b.X, dy=a.Y-b.Y;
    return dx*dx+dy*dy;
}
// Only cells touching the actual foundation are exits, including custom foundations.
CellClass* exitCell(BuildingClass* post,UnitTypeClass* type,CellStruct station) {
    auto origin=post->GetMapCoords();
    auto& map=MapClass::Instance;
    CellClass* best=nullptr;
    int nearest=INT_MAX;
    constexpr CellStruct end{0x7fff,0x7fff};
    for(auto offset=post->GetFoundationData(false);offset && *offset!=end;++offset) {
        for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
            if(!dx && !dy) continue;
            CellStruct at{short(origin.X+offset->X+dx),short(origin.Y+offset->Y+dy)};
            const int distance=distanceSquared(at,station);
            if(distance>=nearest || !map.IsWithinUsableArea(at,false)) continue;
            auto cell=map.TryGetCellAt(at);
            if(!cell || cell->GetBuilding() || cell->GetContent() ||
               !cell->IsClearToMove(type->SpeedType,false,false,-1,type->MovementZone,-1,cell->ContainsBridge())) continue;
            best=cell;
            nearest=distance;
        }
    }
    return best;
}
UnitClass* spawn(BuildingClass* post,UnitTypeClass* type,CellStruct station) {
    auto exit=exitCell(post,type,station);
    if(!exit) return nullptr;
    auto unit=static_cast<UnitClass*>(type->CreateObject(post->Owner));
    if(!unit) return nullptr;
    unit->OnBridge=exit->ContainsBridge();
    if(!unit->Unlimbo(exit->GetCoordsWithBridge(),DirType::North)) {
        unit->UnInit();
        return nullptr;
    }
    post->Owner->RecheckTechTree=true;
    return unit;
}
void direct(UnitClass* unit,CellClass* station,int radius) {
    if(!unit->IsOnMap || unit->InLimbo || unit->Transporter || unit->Absorbed || unit->Deactivated ||
       unit->IsImmobilized || unit->IsUnderEMP() || unit->IsBeingWarpedOut() || unit->IsSinking ||
       unit->IsAttackedByLocomotor) return;
    const auto here=unit->GetMapCoords();
    if(distanceSquared(here,station->MapCoords)<=radius*radius) {
        if(auto threat=GuardPoints::findTarget(unit,station,radius)) {
            if(unit->Target!=threat || (unit->CurrentMission!=Mission::Attack && unit->QueuedMission!=Mission::Attack)) {
                unit->SetTarget(threat);
                unit->QueueMission(Mission::Attack,true);
            }
            return;
        }
    }
    if(unit->Target) unit->SetTarget(nullptr);
    if(here==station->MapCoords) {
        if(unit->Destination) unit->SetDestination(nullptr,true);
        if(unit->CurrentMission!=Mission::Guard && unit->QueuedMission!=Mission::Guard)
            unit->QueueMission(Mission::Guard,true);
    } else if(unit->Destination!=station || (unit->CurrentMission!=Mission::Move && unit->QueuedMission!=Mission::Move)) {
        unit->SetDestination(station,true);
        unit->QueueMission(Mission::Move,true);
    }
}
void update(BuildingClass* post,const Config& config,DWORD frame) {
    if(!post->IsAlive || post->Health<=0 || !post->Owner || post->Owner->Defeated) return;
    auto point=GuardPoints::station(post->UniqueID,config.initialPoint);
    if(point.X<0 || point.Y<0 || !MapClass::Instance.IsWithinUsableArea(point,false)) return;
    auto station=MapClass::Instance.TryGetCellAt(point);
    if(!station) return;
    auto& slot=slots[post->UniqueID];
    const int house=post->Owner->ArrayIndex;
    if(slot.house>=0 && slot.house!=house) {
        slot.child=0;
        slot.readyFrame=frame+config.delay;
    }
    slot.house=house;
    UnitClass* child=nullptr;
    if(slot.child) {
        TargetClass target;
        target.m_ID=slot.child;
        target.m_RTTI=static_cast<unsigned char>(AbstractType::Abstract);
        child=target.As_Unit();
        if(!child || !child->IsAlive || child->Health<=0 || child->Owner!=post->Owner) {
            slot.child=0;
            slot.readyFrame=frame+config.delay;
            child=nullptr;
        }
    }
    if(post->InLimbo || !post->IsOnMap || post->Deactivated || post->IsUnderEMP() ||
       post->IsBeingWarpedOut() || !post->IsPowerOnline() ||
       post->CurrentMission==Mission::Construction || post->CurrentMission==Mission::Selling) return;
    if(!child) {
        if(static_cast<int>(frame-slot.readyFrame)<0) return;
        child=spawn(post,config.type,point);
        if(!child) return;
        slot.child=TargetClass(child).m_ID;
    }
    direct(child,station,config.radius);
}
}
}

DEFINE_HOOK(0x55B4E1,ActSp_GroundGuardFrame,5) {
    BetterGarrison::tick();
    DWORD frame=Unsorted::CurrentFrame;
    if(frame%8) return 0;
    for(auto post:BuildingClass::Array) {
        if(!post) continue;
        if(auto config=GroundGuards::config(post)) GroundGuards::update(post,*config,frame);
    }
    return 0;
}
