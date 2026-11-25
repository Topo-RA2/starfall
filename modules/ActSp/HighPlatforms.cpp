#include <BuildingClass.h>
#include <BulletTypeClass.h>
#include <CCINIClass.h>
#include <DisplayClass.h>
#include <EventClass.h>
#include <FootClass.h>
#include <HouseClass.h>
#include <MapClass.h>
#include <RulesClass.h>
#include <TacticalClass.h>
#include <TargetClass.h>
#include <WeaponTypeClass.h>
#include <WWKeyboardClass.h>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <map>
#include <set>
#include <vector>
#include "../../src/FootMovement.h"
#include "HighPlatforms.h"

namespace HighPlatforms {
namespace {
constexpr int deckSize=4;
constexpr int lift=4;
constexpr int maxLevel=14;
constexpr unsigned mapSide=512;
constexpr wchar_t streamName[]=L"ActSp.HighPlatforms";
constexpr DWORD streamVersion=3;

struct Platform {
    CellStruct anchor;
    int level;
    bool nativeRim;

    CellStruct start() const {
        if(nativeRim) return {static_cast<short>(anchor.X+1),static_cast<short>(anchor.Y+1)};
        // Version-one saves used a single occupied cell at the front corner.
        return {static_cast<short>(anchor.X-deckSize),static_cast<short>(anchor.Y-deckSize+1)};
    }
    bool contains(CellStruct cell) const {
        const auto top=start();
        return cell.X>=top.X && cell.X<top.X+deckSize && cell.Y>=top.Y && cell.Y<top.Y+deckSize;
    }
};
constexpr CellStruct foundationEnd{0x7FFF,0x7FFF};
constexpr int footprintSize=deckSize+2;
constexpr int rimLength=footprintSize*4-4;
enum class FoundationLayout { Disabled, Legacy, Rim };

std::map<const BuildingTypeClass*,FoundationLayout> enabledTypes;
std::map<const TechnoTypeClass*,bool> entryTypes;
std::map<DWORD,Platform> platforms;
struct EntryOrder {
    DWORD platformId;
    CellStruct approach;
};
std::map<DWORD,EntryOrder> entries;
std::set<DWORD> falling;
std::array<BYTE,mapSide*mapSide> rimCells{};
bool refreshSavedCover=false;

bool inMapArray(CellStruct at) {
    return static_cast<unsigned>(at.X)<mapSide && static_cast<unsigned>(at.Y)<mapSide;
}

unsigned cellIndex(CellStruct at) {
    return static_cast<unsigned>(at.Y)*mapSide+static_cast<unsigned>(at.X);
}

bool isRim(CellStruct at) {
    return inMapArray(at) && rimCells[cellIndex(at)]!=0;
}

void markRim(const Platform& platform,bool raised) {
    const auto top=platform.start();
    for(int y=-1;y<=deckSize;++y) for(int x=-1;x<=deckSize;++x) {
        if(x>=0 && x<deckSize && y>=0 && y<deckSize) continue;
        const CellStruct at{static_cast<short>(top.X+x),static_cast<short>(top.Y+y)};
        if(!inMapArray(at)) continue;
        auto& count=rimCells[cellIndex(at)];
        if(raised) ++count;
        else --count;
    }
}

CellClass* playable(CellStruct at) {
    auto& map=MapClass::Instance;
    return inMapArray(at) && map.IsWithinUsableArea(at,false)
        ? map.TryGetCellAt(at):nullptr;
}

FoundationLayout layout(const BuildingTypeClass* type) {
    if(!CCINIClass::INI_Rules) return FoundationLayout::Disabled;
    auto [it,added]=enabledTypes.try_emplace(type,FoundationLayout::Disabled);
    if(added && CCINIClass::INI_Rules->ReadBool(type->ID,"HighPlatform.Enabled",false)) {
        const auto foundation=type->GetFoundationData(true);
        if(!foundation) return it->second;
        if(*foundation==CellStruct{0,0} && foundation[1]==foundationEnd) {
            it->second=FoundationLayout::Legacy;
        } else if(type->GetFoundationWidth()==footprintSize && type->GetFoundationHeight(true)==footprintSize) {
            std::array<bool,footprintSize*footprintSize> cells{};
            int count=0;
            for(auto cell=foundation;*cell!=foundationEnd;++cell) {
                const auto [x,y]=*cell;
                if(x<0 || y<0 || x>=footprintSize || y>=footprintSize ||
                    (x>0 && x<footprintSize-1 && y>0 && y<footprintSize-1) || cells[y*footprintSize+x]) return it->second;
                cells[y*footprintSize+x]=true;
                ++count;
            }
            if(count==rimLength) it->second=FoundationLayout::Rim;
        }
    }
    return it->second;
}

const Platform* deckAt(CellStruct at) {
    for(const auto& [id,platform]:platforms) if(platform.contains(at)) return &platform;
    return nullptr;
}

bool firesUpCliff(const TechnoClass* attacker,AbstractClass* target) {
    if(platforms.empty() || !attacker || !target) return false;
    const auto source=attacker->Transporter?attacker->Transporter:attacker;
    if(source->IsInAir() || target->IsInAir()) return false;

    CellStruct destination;
    if(auto cell=abstract_cast<CellClass*>(target)) destination=cell->MapCoords;
    else if(auto building=abstract_cast<BuildingClass*>(target)) destination=building->GetMapCoords();
    else if(auto unit=abstract_cast<FootClass*>(target)) destination=CellClass::Coord2Cell(unit->GetTargetCoords());
    else return false;

    const auto deck=deckAt(destination);
    if(!deck) return false;
    const auto origin=source->GetCoords();
    return !deck->contains(CellClass::Coord2Cell(origin)) &&
        origin.Z<(deck->level+lift)*Unsorted::LevelHeight;
}

bool blockedByCliff(const WeaponTypeClass* weapon) {
    return weapon && weapon->Projectile && weapon->Projectile->SubjectToCliffs;
}

bool mayRaise(CellStruct anchor,FoundationLayout foundation,const BuildingClass* support=nullptr) {
    auto base=playable(anchor);
    if(!base || base->Level<0 || base->Level>maxLevel-lift || base->SlopeIndex ||
       base->ContainsBridge() || base->ContainsBridgeEx() || deckAt(anchor)) return false;
    const Platform proposed{anchor,base->Level,foundation==FoundationLayout::Rim};
    const auto top=proposed.start();
    for(int y=-1;y<=deckSize;++y) for(int x=-1;x<=deckSize;++x) {
        const CellStruct at{static_cast<short>(top.X+x),static_cast<short>(top.Y+y)};
        auto cell=playable(at);
        if(!cell || isRim(at) || cell->Level!=base->Level || cell->SlopeIndex || cell->ContainsBridge() || cell->ContainsBridgeEx()) return false;
        auto occupant=cell->FirstObject;
        if(occupant && occupant==support) occupant=occupant->NextObject;
        if(occupant || cell->AltObject || cell->Jumpjet ||
            cell->OverlayTypeIndex>=0 || cell->TubeIndex>=0 || cell->Rubble || (cell->OccupationFlags&0x3F) ||
            (cell->LandType!=LandType::Clear && cell->LandType!=LandType::Road)) return false;
    }
    return true;
}

bool allowed(const BuildingTypeClass* type,CellStruct anchor) {
    const auto geometry=layout(type);
    if(isRim(anchor) || (geometry!=FoundationLayout::Disabled && !mayRaise(anchor,geometry))) return false;
    if(platforms.empty()) return true;
    const auto origin=deckAt(anchor);
    auto foundation=type->GetFoundationData(true);
    for(;foundation && *foundation!=foundationEnd;++foundation) {
        const CellStruct at{static_cast<short>(anchor.X+foundation->X),static_cast<short>(anchor.Y+foundation->Y)};
        if(isRim(at) || deckAt(at)!=origin) return false;
    }
    return true;
}

void changeLevel(const Platform& platform,int level) {
    auto& map=MapClass::Instance;
    const auto top=platform.start();
    markRim(platform,level!=platform.level);
    for(int y=0;y<deckSize;++y) for(int x=0;x<deckSize;++x) {
        const CellStruct at{static_cast<short>(top.X+x),static_cast<short>(top.Y+y)};
        if(auto cell=playable(at)) cell->Level=static_cast<char>(level);
    }
    for(int y=-1;y<=deckSize;++y) for(int x=-1;x<=deckSize;++x) {
        const CellStruct at{static_cast<short>(top.X+x),static_cast<short>(top.Y+y)};
        if(auto cell=playable(at)) {
            cell->RecalcAttributes(-1);
            map.ResetZones(at);
            map.RecalculateZones(at);
            map.RecalculateSubZones(at);
            cell->MarkForRedraw();
        }
    }
    map.MarkNeedsRedraw(2);
}

TargetClass objectReference(DWORD id) {
    TargetClass reference;
    reference.m_ID=id;
    reference.m_RTTI=static_cast<unsigned char>(AbstractType::Abstract);
    return reference;
}

const Platform* findPlatform(const BuildingClass* building) {
    if(!building || !building->IsAlive || building->Health<=0 || building->InLimbo || !building->IsOnMap ||
       building->CurrentMission==Mission::Selling) return nullptr;
    const auto it=platforms.find(building->UniqueID);
    return it!=platforms.end() && it->second.anchor==building->GetMapCoords()?&it->second:nullptr;
}

FootClass* groundUnit(TechnoClass* unit) {
    if(!unit || (unit->WhatAmI()!=AbstractType::Infantry && unit->WhatAmI()!=AbstractType::Unit)) return nullptr;
    const auto speed=unit->GetTechnoType()->SpeedType;
    return speed>=SpeedType::Foot && speed!=SpeedType::Winged && speed!=SpeedType::Float && speed!=SpeedType::FloatBeach
        ?static_cast<FootClass*>(unit):nullptr;
}

bool canTransfer(const FootClass* unit) {
    return unit && unit->IsAlive && unit->Health>0 && unit->IsOnMap && !unit->InLimbo && !unit->Transporter &&
        !unit->Absorbed && !unit->IsImmobilized && !unit->IsBeingWarpedOut() && !unit->IsInAir() &&
        !unit->IsFallingDown && !unit->IsSinking && !unit->IsCrashing && !unit->Deactivated && !unit->IsUnderEMP() && !unit->OnBridge &&
        unit->Locomotor && unit->TubeIndex<0 && !unit->IsAttackedByLocomotor && !unit->IsDeploying &&
        !unit->ParasiteEatingMe;
}

bool entryEnabled(const TechnoTypeClass* type) {
    if(!CCINIClass::INI_Rules) return false;
    auto [it,added]=entryTypes.try_emplace(type,true);
    if(added) it->second=CCINIClass::INI_Rules->ReadBool(type->ID,"HighPlatform.Enter",true);
    return it->second;
}

bool canEnter(FootClass* unit, BuildingClass* support, const Platform* platform) {
    return platform && canTransfer(unit) && unit->Owner && unit->Owner==support->Owner &&
        !deckAt(unit->GetMapCoords()) && entryEnabled(support->Type) && entryEnabled(unit->GetTechnoType());
}

bool canExit(const FootClass* unit,const BuildingClass* support,const Platform& platform) {
    return unit && unit->Owner && unit->Owner==support->Owner && platform.contains(unit->GetMapCoords()) && canTransfer(unit);
}

bool hasOccupants(const BuildingClass* support,const Platform& platform) {
    for(auto foot:FootClass::Array)
        if(auto unit=groundUnit(foot); canExit(unit,support,platform)) return true;
    return false;
}

CellClass* transferCell(FootClass* unit,const Platform& platform,CellStruct from,bool onDeck,bool approach=false) {
    const auto* type=unit->GetTechnoType();
    const auto top=platform.start();
    const int first=onDeck?0:-2;
    const int last=onDeck?deckSize-1:deckSize+1;
    CellClass* best=nullptr;
    int bestDistance=INT_MAX;
    for(int y=first;y<=last;++y) for(int x=first;x<=last;++x) {
        if(!onDeck && x!=first && x!=last && y!=first && y!=last) continue;
        const CellStruct at{static_cast<short>(top.X+x),static_cast<short>(top.Y+y)};
        const int dx=at.X-from.X,dy=at.Y-from.Y;
        const int distance=dx*dx+dy*dy;
        if(distance>=bestDistance || isRim(at) || deckAt(at)!=(onDeck?&platform:nullptr)) continue;
        auto cell=playable(at);
        if(!cell || cell->Level!=platform.level+(onDeck?lift:0) || cell->SlopeIndex ||
           cell->ContainsBridge() || cell->ContainsBridgeEx() || cell->GetBuilding() || cell->Jumpjet) continue;
        const bool current=approach && at==from;
        if(!current && (cell->GetContent() ||
           !cell->IsClearToMove(type->SpeedType,false,false,-1,type->MovementZone,-1,false))) continue;
        if(approach && !current) {
            if(!unit->IsInSameZoneAs(cell)) continue;
            if(std::any_of(entries.begin(),entries.end(),[&](const auto& entry) {
                return entry.first!=unit->UniqueID && entry.second.approach==at;
            })) continue;
        }
        best=cell;
        bestDistance=distance;
    }
    return best;
}

void approachPlatform(FootClass* unit,CellClass* cell) {
    FootMovement::stopOrders(unit);
    if(unit->GetMapCoords()!=cell->MapCoords) {
        unit->SetDestination(cell,true);
        unit->QueueMission(Mission::Move,true);
        unit->NextMission();
    }
}

bool transferUnit(FootClass* unit,CellClass* cell) {
    if(!FootMovement::interruptTravel(unit)) return false;
    unit->MarkForRedraw();
    unit->Mark(MarkType::Up);
    unit->SetLocation(cell->GetCoords());
    unit->Mark(MarkType::Down);
    FootMovement::syncCellRegistration(unit);
    unit->SightTimer.Start(0);
    unit->ForceMission(Mission::Guard);
    unit->MarkForRedraw();
    return true;
}

void orderEntry(FootClass* unit,BuildingClass* support,const Platform& platform) {
    if(auto approach=transferCell(unit,platform,unit->GetMapCoords(),false,true)) {
        entries[unit->UniqueID]={support->UniqueID,approach->MapCoords};
        approachPlatform(unit,approach);
    }
}

void orderExit(BuildingClass* support,const Platform& platform) {
    for(auto foot:FootClass::Array) {
        auto unit=groundUnit(foot);
        if(!canExit(unit,support,platform)) continue;
        if(auto landing=transferCell(unit,platform,unit->GetMapCoords(),false)) transferUnit(unit,landing);
    }
}

void tickEntries() {
    for(auto it=entries.begin();it!=entries.end();) {
        auto unit=groundUnit(objectReference(it->first).As_Techno());
        auto support=objectReference(it->second.platformId).As_Building();
        const auto platform=findPlatform(support);
        auto& approach=it->second.approach;
        if(!canEnter(unit,support,platform) ||
           (unit->CurrentMission!=Mission::Move && unit->CurrentMission!=Mission::Guard && unit->CurrentMission!=Mission::Sleep) ||
           (unit->Destination && unit->Destination!=playable(approach))) {
            it=entries.erase(it);
            continue;
        }
        const auto at=unit->GetMapCoords();
        if(at==approach) {
            if(auto landing=transferCell(unit,*platform,at,true); landing && transferUnit(unit,landing)) {
                it=entries.erase(it);
                continue;
            }
            if(unit->CurrentMission!=Mission::Sleep) FootMovement::stopOrders(unit);
        } else if(!unit->Destination && (static_cast<DWORD>(Unsorted::CurrentFrame)+it->first)%15==0) {
            if(auto next=transferCell(unit,*platform,at,false,true)) {
                approach=next->MapCoords;
                approachPlatform(unit,next);
            }
        }
        ++it;
    }
}

void refreshSavedPlatformCover() {
    auto& map=MapClass::Instance;
    std::vector<std::pair<BuildingClass*,CellStruct>> supports;
    for(const auto& [id,platform]:platforms) {
        if(!platform.nativeRim) continue;
        auto building=objectReference(id).As_Building();
        auto cell=playable(platform.anchor);
        if(building && building->IsAlive && building->IsOnMap && !building->InLimbo &&
           building->Type->CanHideThings && cell && cell->GetBuilding()==building)
            supports.emplace_back(building,platform.anchor);
    }
    // Let the engine remove each saved building's own cover contributions.
    // Types are shared, so change the flag only after removing all old registrations.
    for(auto& [building,at]:supports) map.RemoveContentAt(&at,building);
    for(auto type:BuildingTypeClass::Array)
        if(type && type->CanHideThings && layout(type)==FoundationLayout::Rim) type->CanHideThings=false;
    for(auto& [building,at]:supports) map.AddContentAt(&at,building);
    if(!supports.empty()) map.MarkNeedsRedraw(2);
}

void destroy(TechnoClass* object) {
    int damage=object->Health;
    object->ReceiveDamage(&damage,0,RulesClass::Instance->C4Warhead,nullptr,true,false,nullptr);
}

void collapse(const Platform& platform) {
    std::set<DWORD> buildings;
    const auto top=platform.start();
    for(int y=0;y<deckSize;++y) for(int x=0;x<deckSize;++x) {
        const CellStruct at{static_cast<short>(top.X+x),static_cast<short>(top.Y+y)};
        if(auto cell=playable(at)) if(auto building=cell->GetBuilding()) buildings.insert(building->UniqueID);
    }
    for(auto id:buildings) {
        auto building=objectReference(id).As_Building();
        if(building && building->IsAlive && building->Health>0) destroy(building);
    }

    // Collect after destroying the buildings so any ejected occupants on the deck also fall.
    std::vector<TargetClass> occupants;
    for(auto unit:FootClass::Array) {
        if(unit && unit->IsAlive && unit->Health>0 && unit->IsOnMap && !unit->InLimbo && !unit->IsInAir() &&
           platform.contains(unit->GetMapCoords())) occupants.emplace_back(unit);
    }
    changeLevel(platform,platform.level);
    for(auto reference:occupants) if(auto unit=reference.As_Foot();unit && unit->IsAlive && unit->Health>0) {
        falling.insert(unit->UniqueID);
        unit->DropAsBomb();
    }
}

void placed(BuildingClass* building) {
    const auto geometry=layout(building->Type);
    if(geometry==FoundationLayout::Disabled || platforms.contains(building->UniqueID)) return;
    const auto anchor=building->GetMapCoords();
    if(!mayRaise(anchor,geometry,building)) return;
    const Platform platform{anchor,playable(anchor)->Level,geometry==FoundationLayout::Rim};
    platforms.emplace(building->UniqueID,platform);
    changeLevel(platform,platform.level+lift);
}

BuildingClass* wallAt(Point2D point) {
    // This runs after the engine's screen-space lookup of units and other render-layer objects.
    // Resolve the face to the same building object that owns the ground foundation.
    BuildingClass* result=nullptr;
    int nearestBase=0;
    for(const auto& [id,platform]:platforms) {
        const auto top=platform.start();
        const auto middle=playable({static_cast<short>(top.X+deckSize/2),static_cast<short>(top.Y+deckSize/2)});
        if(!middle || middle->IsShrouded() || middle->IsFogged()) continue;
        auto center=middle->GetCoords();
        center.X-=Unsorted::LeptonsPerCell/2;
        center.Y-=Unsorted::LeptonsPerCell/2;
        const auto [projected,visible]=TacticalClass::Instance->CoordsToClient(center);
        if(!visible) continue;
        const int x=std::abs(point.X-projected.X);
        if(x>=deckSize*Unsorted::CellWidthInPixels/2) continue;
        const int front=projected.Y+(deckSize*Unsorted::CellHeightInPixels-x)/2;
        const int base=front+TacticalClass::AdjustForZ(center.Z)-TacticalClass::AdjustForZ(center.Z-lift*Unsorted::LevelHeight);
        if(point.Y<=front || point.Y>=base || (result && base<=nearestBase)) continue;
        auto support=objectReference(id).As_Building();
        if(!support || !support->IsAlive || support->Health<=0 || !support->IsOnMap || support->InLimbo ||
           (support->Owner!=HouseClass::CurrentPlayer && support->CloakState!=CloakState::Uncloaked)) continue;
        result=support;
        nearestBase=base;
    }
    return result;
}

template<class T> bool write(IStream* stream,const T& value) {
    ULONG written=0;
    return SUCCEEDED(stream->Write(&value,sizeof(value),&written)) && written==sizeof(value);
}

template<class T> bool read(IStream* stream,T& value) {
    ULONG count=0;
    return stream->Read(&value,sizeof(value),&count)==S_OK && count==sizeof(value);
}

bool saveState(IStream* stream) {
    if(!write(stream,streamVersion) || !write(stream,static_cast<DWORD>(platforms.size()))) return false;
    for(const auto& [id,platform]:platforms)
        if(!write(stream,id) || !write(stream,platform.anchor) || !write(stream,platform.level) ||
           !write(stream,static_cast<BYTE>(platform.nativeRim))) return false;
    if(!write(stream,static_cast<DWORD>(falling.size()))) return false;
    for(auto id:falling) if(!write(stream,id)) return false;
    if(!write(stream,static_cast<DWORD>(entries.size()))) return false;
    for(const auto& [id,order]:entries)
        if(!write(stream,id) || !write(stream,order.platformId) || !write(stream,order.approach)) return false;
    return SUCCEEDED(stream->Commit(STGC_DEFAULT));
}

bool loadState(IStream* stream) {
    STATSTG info{};
    DWORD version=0,count=0;
    if(FAILED(stream->Stat(&info,STATFLAG_NONAME)) || !read(stream,version) || version<1 || version>streamVersion ||
       !read(stream,count)) return false;
    const auto recordSize=sizeof(DWORD)+sizeof(CellStruct)+sizeof(int)+(version>=2?sizeof(BYTE):0);
    const auto prefix=3*sizeof(DWORD)+static_cast<ULONGLONG>(count)*recordSize;
    if(info.cbSize.QuadPart<prefix) return false;
    std::map<DWORD,Platform> restored;
    std::set<int> occupied;
    for(DWORD i=0;i<count;++i) {
        DWORD id=0;
        Platform platform{};
        BYTE nativeRim=0;
        if(!read(stream,id) || !id || !read(stream,platform.anchor) || !playable(platform.anchor) ||
           !read(stream,platform.level) || platform.level<0 || platform.level>maxLevel-lift ||
           (version>=2 && (!read(stream,nativeRim) || nativeRim>1))) return false;
        platform.nativeRim=nativeRim!=0;
        if(!restored.emplace(id,platform).second) return false;
        const auto top=platform.start();
        for(int y=0;y<deckSize;++y) for(int x=0;x<deckSize;++x) {
            const CellStruct at{static_cast<short>(top.X+x),static_cast<short>(top.Y+y)};
            if(!playable(at) || !occupied.insert(cellIndex(at)).second) return false;
        }
    }
    std::set<DWORD> restoredFalling;
    if(!read(stream,count)) return false;
    const auto afterFalling=prefix+static_cast<ULONGLONG>(count)*sizeof(DWORD);
    if(version>=3 ? info.cbSize.QuadPart<afterFalling+sizeof(DWORD) : info.cbSize.QuadPart!=afterFalling) return false;
    for(DWORD i=0;i<count;++i) {
        DWORD id=0;
        if(!read(stream,id) || !id || !restoredFalling.insert(id).second) return false;
    }
    std::map<DWORD,EntryOrder> restoredEntries;
    if(version>=3) {
        constexpr auto entrySize=2*sizeof(DWORD)+sizeof(CellStruct);
        if(!read(stream,count) || info.cbSize.QuadPart!=afterFalling+sizeof(DWORD)+static_cast<ULONGLONG>(count)*entrySize) return false;
        for(DWORD i=0;i<count;++i) {
            DWORD id=0;
            EntryOrder order{};
            if(!read(stream,id) || !id || !read(stream,order.platformId) || !order.platformId ||
               !read(stream,order.approach) || !playable(order.approach) ||
               !restoredEntries.emplace(id,order).second) return false;
        }
    }
    platforms.swap(restored);
    falling.swap(restoredFalling);
    entries.swap(restoredEntries);
    for(const auto& [id,platform]:platforms) markRim(platform,true);
    return true;
}
}

void tick() {
    if(refreshSavedCover) {
        refreshSavedCover=false;
        refreshSavedPlatformCover();
    }
    for(auto it=platforms.begin();it!=platforms.end();) {
        auto support=objectReference(it->first).As_Building();
        if(support && support->IsAlive && support->Health>0 && support->IsOnMap && !support->InLimbo &&
           support->GetMapCoords()==it->second.anchor && layout(support->Type)!=FoundationLayout::Disabled) {
            ++it;
            continue;
        }
        const auto platform=it->second;
        it=platforms.erase(it);
        collapse(platform);
    }
    tickEntries();
}

void reset() {
    enabledTypes.clear();
    entryTypes.clear();
    platforms.clear();
    entries.clear();
    falling.clear();
    rimCells.fill(0);
    refreshSavedCover=false;
}
void forgetEntry(DWORD id) { entries.erase(id); }
void forget(DWORD id) {
    falling.erase(id);
    forgetEntry(id);
}

bool save(IStorage* storage) {
    IStream* stream=nullptr;
    if(FAILED(storage->CreateStream(streamName,STGM_CREATE|STGM_WRITE|STGM_SHARE_EXCLUSIVE,0,0,&stream))) return false;
    const bool ok=saveState(stream);
    stream->Release();
    return ok;
}

bool load(IStorage* storage) {
    reset();
    IStream* stream=nullptr;
    const HRESULT status=storage->OpenStream(streamName,nullptr,STGM_READ|STGM_SHARE_EXCLUSIVE,0,&stream);
    if(status==STG_E_FILENOTFOUND) return true;
    if(FAILED(status)) return false;
    const bool ok=loadState(stream);
    stream->Release();
    refreshSavedCover=ok;
    return ok;
}
}

using namespace HighPlatforms;

namespace {
bool normalPointer(bool ignoreForce) {
    const auto keyboard=WWKeyboardClass::Instance;
    return ignoreForce || (!keyboard->IsForceFireKeyPressed() && !keyboard->IsForceMoveKeyPressed() &&
        !keyboard->IsForceSelectKeyPressed());
}

bool sendTransfer(TechnoClass* sender,Mission mission,BuildingClass* support) {
    if(!sender->Owner || !sender->Owner->IsControlledByCurrentPlayer()) return false;
    const TargetClass target(support);
    // Enter and Unload use the same stock network packet as ordinary unit orders.
    const EventClass order(sender->Owner->ArrayIndex,TargetClass(sender),mission,target,target,TargetClass{});
    return EventClass::OutList.Add(order);
}

bool canDeployPlatform(BuildingClass* support) {
    const auto platform=findPlatform(support);
    return platform && support->Owner && support->Owner->IsControlledByCurrentPlayer() && hasOccupants(support,*platform);
}

Action enterPointer(REGISTERS* registers) {
    if(platforms.empty() || !normalPointer(registers->Stack<bool>(8))) return Action::None;
    auto support=abstract_cast<BuildingClass*>(registers->Stack<ObjectClass*>(4));
    auto unit=groundUnit(registers->ECX<TechnoClass*>());
    const auto platform=findPlatform(support);
    if(!platform || !canTransfer(unit) || !unit->Owner || unit->Owner!=support->Owner ||
       deckAt(unit->GetMapCoords())) return Action::None;
    return entryEnabled(support->Type) && entryEnabled(unit->GetTechnoType())?Action::Enter:Action::NoEnter;
}

bool clickEnter(REGISTERS* registers) {
    const auto action=registers->Stack<Action>(4);
    if(action!=Action::Enter && action!=Action::NoEnter) return false;
    auto support=abstract_cast<BuildingClass*>(registers->Stack<ObjectClass*>(8));
    const auto platform=findPlatform(support);
    if(!platform) return false;
    auto unit=groundUnit(registers->ECX<TechnoClass*>());
    registers->EAX(action==Action::Enter && canEnter(unit,support,platform) &&
        sendTransfer(unit,Mission::Enter,support)?1u:0u);
    return true;
}
}

DEFINE_HOOK(0x51E3B0,ActSp_InfantryPlatformEnterPointer,7) {
    const auto action=enterPointer(R);
    if(action==Action::None) return 0;
    R->EAX(action);
    return 0x51E45F;
}

DEFINE_HOOK(0x73FD50,ActSp_VehiclePlatformEnterPointer,7) {
    const auto action=enterPointer(R);
    if(action==Action::None) return 0;
    R->EAX(action);
    return 0x73FD78;
}

DEFINE_HOOK(0x51F190,ActSp_InfantryClickPlatformEnter,5) {
    return clickEnter(R)?0x51F1F7:0;
}

DEFINE_HOOK(0x738890,ActSp_VehicleClickPlatformEnter,5) {
    return clickEnter(R)?0x73890A:0;
}

DEFINE_HOOK(0x447210,ActSp_PlatformDeployPointer,6) {
    auto support=R->ECX<BuildingClass*>();
    if(support!=R->Stack<ObjectClass*>(4) || !normalPointer(R->Stack<bool>(8)) || !canDeployPlatform(support)) return 0;
    R->EAX(Action::Self_Deploy);
    return 0x447230;
}

DEFINE_HOOK(0x443450,ActSp_ClickPlatformDeploy,9) {
    if(R->EAX<Action>()!=Action::Self_Deploy) return 0;
    auto support=R->EBX<BuildingClass*>();
    if(!findPlatform(support)) return 0;
    R->EAX(canDeployPlatform(support) && sendTransfer(support,Mission::Unload,support)?1u:0u);
    return 0x443444;
}

DEFINE_HOOK(0x730AF0,ActSp_PlatformDeployKey,8) {
    if(platforms.empty()) return 0;
    for(auto selected:ObjectClass::CurrentObjects) {
        auto support=abstract_cast<BuildingClass*>(selected);
        if(canDeployPlatform(support)) sendTransfer(support,Mission::Unload,support);
    }
    return 0;
}

DEFINE_HOOK(0x4C7217,ActSp_ApplyPlatformTransfer,8) {
    if(platforms.empty() && entries.empty()) return 0;
    auto sender=R->EDI<TechnoClass*>();
    auto event=R->ESI<EventClass*>();
    if(!sender->Owner || event->HouseIndex!=sender->Owner->ArrayIndex || sender->InLimbo) return 0;
    const auto& order=event->MegaMission;
    const auto mission=static_cast<Mission>(order.Mission);
    if(event->Type==EventType::MegaMission && order.Target.m_RTTI==static_cast<BYTE>(AbstractType::Abstract) &&
       order.Target.m_ID==order.Destination.m_ID && order.Target.m_RTTI==order.Destination.m_RTTI) {
        auto support=event->MegaMission.Target.As_Building();
        if(mission==Mission::Enter) {
            forgetEntry(sender->UniqueID);
            auto unit=groundUnit(sender);
            if(const auto platform=findPlatform(support); canEnter(unit,support,platform)) orderEntry(unit,support,*platform);
            return 0x4C8109;
        }
        if(mission==Mission::Unload && order.Target.m_ID==sender->UniqueID) {
            if(const auto platform=findPlatform(support)) {
                orderExit(support,*platform);
                return 0x4C8109;
            }
        }
    }
    forgetEntry(sender->UniqueID);
    return 0;
}

DEFINE_HOOK(0x464AC0,ActSp_CheckPlatformPlacement,6) {
    const auto cell=R->Stack<const CellStruct*>(4);
    if(cell && !allowed(R->ECX<BuildingTypeClass*>(),*cell)) {
        R->EAX(0);
        return 0x464ACC;
    }
    return 0;
}

DEFINE_HOOK(0x4A8EB0,ActSp_CheckPlatformPreview,5) {
    auto type=abstract_cast<BuildingTypeClass*>(R->Stack<ObjectTypeClass*>(4));
    const auto cell=R->Stack<const CellStruct*>(0x10);
    if(type && cell && *cell!=CellStruct::Empty && !allowed(type,*cell)) {
        R->EAX(0);
        return 0x4A9063;
    }
    return 0;
}

DEFINE_HOOK(0x4834A0,ActSp_CheckPlatformPathCell,5) {
    const auto cell=R->ECX<const CellClass*>();
    if(R->Stack<SpeedType>(4)!=SpeedType::Winged && isRim(cell->MapCoords)) {
        R->EAX(0);
        return 0x4834DE;
    }
    return 0;
}

bool cannotEnterPlatformRim(REGISTERS* registers) {
    const auto cell=registers->Stack<const CellClass*>(4);
    if(!cell || !isRim(cell->MapCoords)) return false;
    const auto unit=registers->ECX<const FootClass*>();
    return !unit->IsOnMap || !unit->IsInAir();
}

DEFINE_HOOK(0x51BF90,ActSp_CheckPlatformInfantryCell,5) {
    if(cannotEnterPlatformRim(R)) {
        R->EAX(static_cast<int>(Move::No));
        return 0x51C036;
    }
    return 0;
}

DEFINE_HOOK(0x73F0A0,ActSp_CheckPlatformVehicleCell,6) {
    if(cannotEnterPlatformRim(R)) {
        R->EAX(static_cast<int>(Move::No));
        return 0x73F18E;
    }
    return 0;
}

DEFINE_HOOK(0x440580,ActSp_CheckPlatformUnlimbo,5) {
    const auto coords=R->Stack<const CoordStruct*>(4);
    if(coords && !allowed(R->ECX<BuildingClass*>()->Type,CellClass::Coord2Cell(*coords))) {
        R->EAX(0);
        return 0x44086E;
    }
    return 0;
}

DEFINE_HOOK(0x440D01,ActSp_RaisePlatform,6) {
    placed(R->ESI<BuildingClass*>());
    return 0;
}

DEFINE_HOOK(0x449410,ActSp_PlatformRenderOrder,8) {
    auto building=R->ECX<BuildingClass*>();
    if(building->Type && layout(building->Type)==FoundationLayout::Rim) {
        const auto at=building->GetMapCoords();
        R->EAX((at.X+at.Y)*Unsorted::LeptonsPerCell);
        return 0x44943D;
    }
    return 0;
}

DEFINE_HOOK_AGAIN(0x706458,ActSp_PlatformDrawDepth,5)
DEFINE_HOOK(0x70652D,ActSp_PlatformDrawDepth,5) {
    auto building=abstract_cast<BuildingClass*>(R->ESI<TechnoClass*>());
    if(building && layout(building->Type)==FoundationLayout::Rim) {
        // The platform has already been sorted behind its occupants. Its body
        // must not leave a building depth mask over the walkable deck.
        constexpr unsigned depthFlags=0x7000;
        R->Stack(0x10,R->Stack<unsigned>(0x10)&~depthFlags);
        R->Stack<SHPStruct*>(0x28,nullptr);
    }
    return 0;
}

DEFINE_HOOK(0x6DA4B8,ActSp_FindPlatformWall,6) {
    if(!platforms.empty() && !DisplayClass::Instance.CurrentBuilding) {
        if(auto building=wallAt(*R->ESI<const Point2D*>())) {
            R->EAX(building);
            return 0x6DA501;
        }
    }
    return 0;
}

DEFINE_HOOK(0x6FC339,ActSp_CheckPlatformCliffAttack,6) {
    if(blockedByCliff(R->EDI<WeaponTypeClass*>()) &&
       firesUpCliff(R->ESI<TechnoClass*>(),R->Stack<AbstractClass*>(0x24))) return 0x6FC86A;
    return 0;
}

DEFINE_HOOK(0x6FDDC0,ActSp_CheckPlatformCliffFire,6) {
    if(blockedByCliff(R->EBX<WeaponTypeClass*>()) &&
       firesUpCliff(R->ESI<TechnoClass*>(),R->EDI<AbstractClass*>())) return 0x6FDE03;
    return 0;
}

DEFINE_HOOK(0x6F86FE,ActSp_CheckPlatformCliffThreat,6) {
    const auto attacker=R->EDI<TechnoClass*>();
    const auto target=R->ESI<ObjectClass*>();
    if(firesUpCliff(attacker,target)) {
        const int slot=attacker->SelectWeapon(target);
        const auto mount=slot>=0?attacker->GetWeapon(slot):nullptr;
        if(mount && blockedByCliff(mount->WeaponType)) return 0x6F894F;
    }
    return 0;
}

// Retain native falling motion and override damage only when a platform victim reaches the ground.
DEFINE_HOOK(0x5F4021,ActSp_PlatformFallLanding,6) {
    auto object=R->ESI<ObjectClass*>();
    if(!falling.erase(object->UniqueID)) return 0;
    object->FallRate=0;
    object->IsABomb=false;
    if(auto unit=abstract_cast<FootClass*>(object);unit && unit->IsAlive && unit->Health>0) destroy(unit);
    return 0x5F405B;
}
