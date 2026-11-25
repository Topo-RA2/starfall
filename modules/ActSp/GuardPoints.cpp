#include <HouseClass.h>
#include <EventClass.h>
#include <MapClass.h>
#include <map>
#include <set>
#include "GuardPoints.h"
#include "GroundGuards.h"
#include "BetterGarrison.h"

namespace {
std::map<DWORD,CellStruct> overrides;
constexpr wchar_t streamName[]=L"ActSp.GuardPoints";
constexpr DWORD formatVersion=4;

bool playable(CellStruct cell) {
    return cell.X>=0 && cell.X<512 && cell.Y>=0 && cell.Y<512 &&
           MapClass::Instance.IsWithinUsableArea(cell,false) && MapClass::Instance.TryGetCellAt(cell);
}
bool setPoint(TechnoClass* owner,CellStruct cell) {
    if(!GuardPoints::eligible(owner) || !playable(cell)) return false;
    overrides[owner->UniqueID]=cell;
    return true;
}
bool needsRallyCommand(BuildingClass* building) {
    return building->Owner && building->Owner->IsControlledByCurrentPlayer() &&
           !building->IsUnitFactory() && GuardPoints::eligible(building);
}

struct SavedPoint { DWORD owner; CellStruct cell; };
static_assert(sizeof(SavedPoint)==8);
struct SavedGround { DWORD post; GroundGuards::Slot slot; };
static_assert(sizeof(SavedGround)==16);
struct SavedObjectOrder { DWORD post; int house; int target; };
static_assert(sizeof(SavedObjectOrder)==12);
bool writePoints(IStream* stream) {
    auto write=[stream](const auto& value) {
        ULONG size=0;
        return SUCCEEDED(stream->Write(&value,sizeof(value),&size)) && size==sizeof(value);
    };
    const DWORD count=static_cast<DWORD>(overrides.size());
    if(!write(formatVersion) || !write(count)) return false;
    for(const auto& [owner,cell]:overrides)
        if(!write(SavedPoint{owner,cell})) return false;
    const DWORD guards=static_cast<DWORD>(GroundGuards::slots.size());
    if(!write(guards)) return false;
    for(const auto& [post,slot]:GroundGuards::slots)
        if(!write(SavedGround{post,slot})) return false;
    const DWORD orders=static_cast<DWORD>(BetterGarrison::orders.size());
    if(!write(orders)) return false;
    for(const auto& [post,order]:BetterGarrison::orders)
        if(!write(SavedObjectOrder{post,order.house,order.target}) || !write(static_cast<DWORD>(order.kind))) return false;
    return SUCCEEDED(stream->Commit(STGC_DEFAULT));
}
bool readPoints(IStream* stream) {
    auto read=[stream](auto& value) {
        ULONG size=0;
        return stream->Read(&value,sizeof(value),&size)==S_OK && size==sizeof(value);
    };
    DWORD version=0,count=0;
    STATSTG info{};
    if(!read(version) || version<1 || version>formatVersion || !read(count) ||
       FAILED(stream->Stat(&info,STATFLAG_NONAME))) return false;
    auto prefix=2*sizeof(DWORD)+static_cast<ULONGLONG>(count)*sizeof(SavedPoint);
    if(info.cbSize.QuadPart<prefix || (version==1 && info.cbSize.QuadPart!=prefix)) return false;
    std::map<DWORD,CellStruct> restored;
    for(DWORD i=0;i<count;++i) {
        SavedPoint entry{};
        if(!read(entry) || !entry.owner || !playable(entry.cell) ||
           !restored.emplace(entry.owner,entry.cell).second) return false;
    }
    std::map<DWORD,GroundGuards::Slot> slots;
    if(version>=2) {
        DWORD guards=0;
        if(!read(guards)) return false;
        prefix+=sizeof(DWORD)+static_cast<ULONGLONG>(guards)*sizeof(SavedGround);
        if(info.cbSize.QuadPart<prefix || (version==2 && info.cbSize.QuadPart!=prefix)) return false;
        std::set<int> children;
        for(DWORD i=0;i<guards;++i) {
            SavedGround entry{};
            if(!read(entry) || !entry.post || entry.slot.house<0 || entry.slot.house>=HouseClass::Array.Count ||
               (entry.slot.child && !children.insert(entry.slot.child).second) ||
               !slots.emplace(entry.post,entry.slot).second) return false;
        }
    }
    std::map<DWORD,BetterGarrison::Order> orders;
    if(version>=3) {
        DWORD count=0;
        const auto orderSize=sizeof(SavedObjectOrder)+(version>=4?sizeof(DWORD):0);
        if(!read(count) || info.cbSize.QuadPart!=prefix+sizeof(DWORD)+static_cast<ULONGLONG>(count)*orderSize) return false;
        for(DWORD i=0;i<count;++i) {
            SavedObjectOrder entry{};
            DWORD kind=static_cast<DWORD>(AbstractType::Abstract);
            if(!read(entry) || (version>=4 && !read(kind)) || !entry.post ||
               entry.house<0 || entry.house>=HouseClass::Array.Count) return false;
            if(kind==static_cast<DWORD>(AbstractType::Cell)) {
                if(entry.target<0 || entry.target>=512000 || entry.target%1000>=512 ||
                   !playable({static_cast<short>(entry.target%1000),static_cast<short>(entry.target/1000)})) return false;
            } else if(kind!=static_cast<DWORD>(AbstractType::Abstract) || !entry.target) return false;
            if(!orders.emplace(entry.post,BetterGarrison::Order{entry.house,entry.target,static_cast<AbstractType>(kind)}).second) return false;
        }
    }
    overrides.swap(restored);
    GroundGuards::slots.swap(slots);
    BetterGarrison::orders.swap(orders);
    return true;
}
}

CellStruct GuardPoints::station(DWORD ownerId,CellStruct fallback) {
    auto found=overrides.find(ownerId);
    return found==overrides.end()?fallback:found->second;
}
void GuardPoints::reset() { overrides.clear(); GroundGuards::slots.clear(); BetterGarrison::reset(); }

// The Starfall Lua host resolves this optional export only for a live unit handle.
extern "C" __declspec(dllexport) BOOL __cdecl ActSp_SetDroneGuardCell(TechnoClass* owner,int x,int y) {
    if(x<0 || x>=512 || y<0 || y>=512) return FALSE;
    return setPoint(owner,{static_cast<short>(x),static_cast<short>(y)});
}

// BuildingClass::MouseOverCell has already decided on attack/force-fire and other special actions.
DEFINE_HOOK(0x44776D,ActSp_GuardRallyAction,5) {
    auto action=R->EBX<Action>();
    if(action!=Action::None && action!=Action::Move && action!=Action::NoMove) return 0;
    auto building=R->ESI<BuildingClass*>();
    auto cell=R->Stack<const CellStruct*>(0x1C);
    if(cell && needsRallyCommand(building) && playable(*cell)) R->EBX(Action::Move);
    return 0;
}
// Non-factory outposts use the same synchronized Archive event as native factory rally points.
DEFINE_HOOK(0x44373D,ActSp_ClickGuardRally,6) {
    auto building=R->ESI<BuildingClass*>();
    auto cell=R->Stack<const CellStruct*>(0x8C);
    if(R->EAX<Action>()!=Action::Move || !needsRallyCommand(building)) return 0;
    if(cell && playable(*cell)) {
        EventClass order(building->Owner->ArrayIndex,EventType::Archive,TargetClass(building),TargetClass(*cell));
        EventClass::OutList.Add(order);
    }
    return 0x44384E;
}
// Only the execution of the network event changes the guard point, never local mouse input.
DEFINE_HOOK(0x4C6DD3,ActSp_ApplyGuardRally,8) {
    auto owner=R->EDI<TechnoClass*>();
    auto& target=R->ESI<EventClass*>()->Archive.Whom2;
    if(owner->WhatAmI()==AbstractType::Building && owner->CurrentMission!=Mission::Selling &&
       target.m_RTTI==static_cast<unsigned char>(AbstractType::Cell)) {
        if(auto cell=target.As_Cell()) setPoint(owner,cell->MapCoords);
    }
    return 0;
}

DEFINE_HOOK(0x6F4500,ActSp_ForgetGuardPoint,5) {
    auto id=R->ECX<TechnoClass*>()->UniqueID;
    overrides.erase(id);
    GroundGuards::slots.erase(id);
    BetterGarrison::orders.erase(id);
    return 0;
}
DEFINE_HOOK(0x67D074,ActSp_SaveGuardPoints,6) {
    auto storage=R->EAX<IStorage*>();
    IStream* stream=nullptr;
    if(FAILED(storage->CreateStream(streamName,STGM_CREATE|STGM_WRITE|STGM_SHARE_EXCLUSIVE,0,0,&stream)))
        return 0x67D08F;
    bool ok=writePoints(stream);
    stream->Release();
    return ok?0:0x67D08F;
}
DEFINE_HOOK(0x67E67B,ActSp_LoadGuardPoints,5) {
    GuardPoints::clearProfiles();
    BetterGarrison::reset();
    auto storage=R->Stack<IStorage*>(0x18);
    IStream* stream=nullptr;
    HRESULT status=storage->OpenStream(streamName,nullptr,STGM_READ|STGM_SHARE_EXCLUSIVE,0,&stream);
    if(status==STG_E_FILENOTFOUND) { GuardPoints::reset(); return 0; }
    bool ok=SUCCEEDED(status) && readPoints(stream);
    if(stream) stream->Release();
    if(!ok) {
        MessageBoxW(nullptr,L"无法读取子机警戒格。请检查存档和 ActSp 版本。",L"ActSp",MB_OK|MB_ICONERROR);
        ExitProcess(EXIT_FAILURE);
    }
    return 0;
}
