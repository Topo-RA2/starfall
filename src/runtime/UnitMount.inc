// Relations are keyed by game object ID. Pointers are never persisted.
namespace Mount {
using UnitMount::links;
using UnitMount::Link;
bool rebind=false;
bool permits(TechnoClass* recipient,TechnoClass* source) {
    if(!UnitMount::mounted(recipient)) return true;
    return source && links.at(recipient->UniqueID).parentId==source->UniqueID;
}
void bindLoaded() {
    if(!rebind) return;
    std::map<DWORD,TechnoClass*> lookup;
    for(auto u:TechnoClass::Array) if(u) lookup.emplace(u->UniqueID,u);
    for(auto& [id,l]:links) {
        l.parent=lookup[l.parentId];
        l.child=abstract_cast<FootClass*>(lookup[id]);
        l.ordered=l.orderId?lookup[l.orderId]:nullptr;
        if(l.child) l.trackerIndex=ForceMove::locateTracker(l.child);
    }
    rebind=false;
}
bool occupied(DWORD parent,const string& slot) {
    return !slot.empty() && std::any_of(links.begin(),links.end(),[&](const auto& entry){
        return entry.second.parentId==parent && entry.second.slot==slot;
    });
}
bool placement(TechnoClass* parent,const PassiveAction& a,Link& l) {
    const string section=parent->GetTechnoType()->ID;
    const auto slots=split(read(section,"AttachSlots"));
    auto node=a.mountNode;
    if(a.mountSlot=="auto") {
        for(const auto& slot:slots) if(!occupied(parent->UniqueID,slot)) {node=slot;break;}
        if(node.empty()) return false;
    }
    l.slot=node.empty()?a.mountSlot:node;
    if(occupied(parent->UniqueID,l.slot)) return false;
    l.offset=a.mountOffset;
    l.turret=a.mountAnchor=="turret";
    if(node.empty()) return true;
    const auto prefix="AttachNode."+node;
    const auto offset=read(section,(prefix+".FLH").c_str());
    const auto anchor=read(section,(prefix+".Anchor").c_str());
    if(anchor.empty() && offset.empty() && !includes(slots,node)) return false;
    if(!anchor.empty() && anchor!="body" && anchor!="turret") return false;
    CoordStruct base{};
    if(!offset.empty() && !mountOffset(offset,base)) return false;
    l.turret=anchor=="turret";l.offset+=base;
    return true;
}
CoordStruct position(const Link& l) {
    auto p=l.parent;
    const bool building=p->WhatAmI()==AbstractType::Building;
    CoordStruct at=building?p->GetCenterCoords():p->Location;
    if(auto m=ForceMove::motions.find(l.parentId);m!=ForceMove::motions.end()) at.Z+=m->second.lift();
    const double scale=VisualScale::percent(l.parentId)/100.0;
    // Buildings use their primary facing for aiming. Their foundation never turns.
    const auto body=(building?DirStruct(DirType::North):p->PrimaryFacing.Current()).GetRadian<256>();
    const bool turret=l.turret && (!building || p->GetTechnoType()->Turret);
    const auto facing=turret?p->TurretFacing().GetRadian<256>():body;
    const int pivot=turret?p->GetTechnoType()->TurretOffset:0;
    at.X+=int(std::lround(scale*(pivot*std::cos(body)+l.offset.X*std::cos(facing)-l.offset.Y*std::sin(facing))));
    at.Y-=int(std::lround(scale*(pivot*std::sin(body)+l.offset.X*std::sin(facing)+l.offset.Y*std::cos(facing))));
    at.Z+=int(std::lround(scale*l.offset.Z));return at;
}
bool emerge(FootClass* child,TechnoClass* parent,CoordStruct at) {
    if(!MapClass::Instance.TryGetCellAt(CellClass::Coord2Cell(at))) return false;
    child->OnBridge=parent->OnBridge;
    ++Unsorted::ScenarioInit;
    bool placed=child->Unlimbo(at,DirType::North);
    --Unsorted::ScenarioInit;
    if(placed) child->ForceMission(Mission::Guard);
    return placed;
}
bool create(TechnoClass* parent,const PassiveAction& a) {
    if(!alive(parent) || !parent->Owner || !parent->IsOnMap || parent->InLimbo ||
       UnitMount::mounted(parent)) return false;
    bindLoaded();
    Link l;
    if(!placement(parent,a,l)) return false;
    l.parent=parent;l.parentId=parent->UniqueID;l.excluded=a.excludeParentPassives;l.transportFire=a.transportFire;
    l.child=static_cast<FootClass*>(a.unitType->CreateObject(parent->Owner));
    if(!l.child) return false;
    auto child=l.child;
    auto id=child->UniqueID;
    links.emplace(id,l); // Occupation hooks must already know the child during Unlimbo.
    if(emerge(child,parent,position(l))) return true;
    child->UnInit();links.erase(id);return false;
}
void trackAir(Link& l,bool flying) {
    if(!flying && l.trackerIndex<0) return;
    ForceMove::Motion tracking;
    tracking.foot=l.child;tracking.duration=1;
    tracking.nativeTracked=flying;tracking.trackerIndex=l.trackerIndex;
    ForceMove::syncAirTracker(tracking);
    l.trackerIndex=tracking.trackerIndex;
}
void forget(DWORD id) {
    if(links.empty()) return;
    if(auto it=links.find(id);it!=links.end()) {trackAir(it->second,false);links.erase(it);}
    for(auto& [child,l]:links) {
        if(l.parentId==id) l.parent=nullptr;
        if(l.orderId==id) {l.ordered=nullptr;l.orderId=0;}
    }
}
bool removeAttached(TechnoClass* parent,const PassiveAction& action) {
    if(!alive(parent) || links.empty()) return false;
    bindLoaded();
    // Installing always creates a new native object. Its monotonically assigned
    // ID preserves installation order in existing saves as well as new games.
    int removed=0;
    for(auto it=links.end();it!=links.begin();) {
        --it;
        const auto& link=it->second;
        auto child=link.child;
        if(link.parent!=parent || !alive(child)) continue;
        if(!action.mountSlot.empty() && link.slot!=action.mountSlot) continue;
        if(action.unitType && child->GetTechnoType()!=action.unitType) continue;
        auto id=it->first;
        if(child->InLimbo || child->Limbo()) {
            child->UnInit(); // The native cleanup hook releases the slot immediately.
            if(++removed==action.removeMountCount) break;
        }
        // Cleanup can invalidate multiple iterators. Resume at the next older ID.
        it=links.lower_bound(id);
    }
    return removed>0;
}
void order(TechnoClass* source,TechnoClass* target) {
    if(!source) return;
    for(auto& [id,l]:links) {
        if(!l.parent) continue;
        auto controller=l.parent->Transporter?l.parent->Transporter:l.parent;
        if(source!=controller) continue;
        l.ordered=target;l.orderId=target?target->UniqueID:0;
        l.carrierId=l.parent->Transporter?controller->UniqueID:0;
    }
}
bool usable(FootClass* child,TechnoClass* target) {
    if(!alive(target) || !target->Owner || target->InLimbo || !target->IsOnMap || child==target) return false;
    if(target->Disguised && !target->IsClearlyVisibleTo(child->Owner)) return false;
    if(target->CloakState!=CloakState::Uncloaked && !child->Owner->IsAlliedWith(target->Owner)) {
        auto cell=target->GetCell();
        auto house=child->Owner->ArrayIndex;
        if(!cell || house<0 || house>=24 || !cell->Sensors_InclHouse(house)) return false;
    }
    auto slot=child->SelectWeapon(target);
    if(slot<0 || !child->IsCloseEnough(target,slot)) return false;
    auto error=child->GetFireError(target,slot,false);
    return error==FireError::OK || error==FireError::REARM || error==FireError::ROTATING || error==FireError::FACING;
}
TechnoClass* acquire(FootClass* child);
void update(Link& l) {
    auto child=l.child;auto parent=l.parent;
    if(child->Owner!=parent->Owner) {
        child->SetOwningHouse(parent->Owner,false);
        child->SetTarget(nullptr);
        l.ordered=nullptr;l.orderId=0;
    }
    if(child->IsSelected) child->Deselect();
    auto carrier=parent->Transporter;
    const DWORD carrierId=carrier?carrier->UniqueID:0;
    if(l.carrierId!=carrierId) {l.carrierId=carrierId;l.orderId=0;l.ordered=nullptr;child->SetTarget(nullptr);}
    bool hidden=carrier || !parent->IsOnMap || parent->InLimbo || parent->CloakState!=CloakState::Uncloaked || parent->Disguised;
    if(hidden) {
        trackAir(l,false);
        if(!child->InLimbo) child->Limbo();
        if(!carrier || parent->CloakState!=CloakState::Uncloaked || parent->Disguised || !l.transportFire || !carrier->GetTechnoType()->OpenTopped || carrier->InLimbo ||
           carrier->CloakState!=CloakState::Uncloaked || carrier->Disguised || child->Deactivated || child->IsUnderEMP()) return;
        // A virtual passenger occupies no seat. Reuse the transport's firing
        // origin for target checks and the native Fire call only.
        child->Location=carrier->Location;
        child->Transporter=carrier;child->InOpenToppedTransport=true;
        auto aim=l.ordered;
        if(!usable(child,aim)) aim=abstract_cast<TechnoClass*>(child->Target);
        if(!usable(child,aim) && (Unsorted::CurrentFrame+child->UniqueID)%8==0)
            aim=acquire(child);
        if(usable(child,aim)) {
            if(child->Target!=aim) child->SetTarget(aim);
            auto slot=child->SelectWeapon(aim);
            auto facing=child->GetTargetDirection(aim);
            child->PrimaryFacing.SetCurrent(facing);child->SecondaryFacing.SetCurrent(facing);
            if(child->GetFireError(aim,slot,false)==FireError::OK) {
                auto id=child->UniqueID;
                child->Fire(aim,slot);
                if(!links.contains(id)) return;
            }
        }
        child->Transporter=nullptr;child->InOpenToppedTransport=false;
        return;
    }
    auto at=position(l);
    if(child->InLimbo && !emerge(child,parent,at)) return;
    if(!MapClass::Instance.TryGetCellAt(CellClass::Coord2Cell(at))) at=parent->Location;
    if(child->Location!=at || child->OnBridge!=parent->OnBridge) {
        child->Mark(MarkType::Up);child->OnBridge=parent->OnBridge;child->SetLocation(at);
        child->Mark(MarkType::Down);ForceMove::syncCellRegistration(child);
    }
    trackAir(l,parent->IsInAir());
    TechnoClass* aim=l.ordered;
    if(!usable(child,aim)) {
        aim=abstract_cast<TechnoClass*>(child->Target);
        if(!usable(child,aim)) aim=(Unsorted::CurrentFrame+child->UniqueID)%8==0?acquire(child):nullptr;
    }
    if(aim && child->Target!=aim) {
        child->SetTarget(aim);child->QueueMission(Mission::Attack,false);
    } else if(!aim && child->Target) {
        child->SetTarget(nullptr);child->QueueMission(Mission::Guard,false);
    }
}
void tick() {
    bindLoaded();
    vector<DWORD> ids;
    for(const auto& [id,l]:links) ids.push_back(id);
    for(auto id:ids) {
        auto it=links.find(id);if(it==links.end()) continue;
        auto& l=it->second;
        if(!l.child) {links.erase(it);continue;}
        if(!alive(l.child)) {
            // Keep the occupation guard until the native death animation finishes.
            l.slot.clear();trackAir(l,false);continue;
        }
        if(!alive(l.parent)) {
            auto child=l.child;
            trackAir(l,false);child->Limbo();child->UnInit();links.erase(id);continue;
        }
        update(l);
    }
}
}
