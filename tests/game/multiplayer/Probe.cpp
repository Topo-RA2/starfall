#include "../../../src/Starfall.cpp"
#include <MessageListClass.h>
#include <sstream>
#include <iomanip>
#include <TacticalClass.h>
#include <Surface.h>
#include "../WindowMouse.h"
EXPORT_FUNC(SF_ResearchDraw);

namespace MultiplayerTest {
std::vector<HouseClass*> players;
std::array<TechnoClass*,2> casters{},targets{};
std::array<int,2> credits{},health{};
std::array<int,2> buildingCounts{};
std::array<CellStruct,2> starts{};
int skillIndex=-1;
void check(bool ok,const char* message) {
    std::ofstream("multiplayer.log",std::ios::app)<<(ok?"PASS ":"FAIL ")<<message<<'\n';
    if(!ok) TerminateProcess(GetCurrentProcess(),2);
}
unsigned long long hash(const std::string& data) {
    unsigned long long value=14695981039346656037ull;
    for(unsigned char c:data) {value^=c;value*=1099511628211ull;}
    return value;
}
void snapshot(int frame) {
    std::ostringstream native;
    const auto& random=ScenarioClass::Instance->Random;
    native<<random.Next1<<' '<<random.Next2;
    for(auto n:random.Table) native<<' '<<n;
    native<<'\n';
    for(auto h:HouseClass::Array) if(h) native<<"H "<<h->ArrayIndex<<' '<<h->Type->ID<<' '
        <<h->IsHumanPlayer<<' '<<h->Defeated<<' '<<h->Available_Money()<<' '
        <<hash(std::string(reinterpret_cast<const char*>(h->ThreatPosedEstimates),sizeof(h->ThreatPosedEstimates)))<<'\n';
    std::map<DWORD,TechnoClass*> objects;
    for(auto t:TechnoClass::Array) if(t) objects.emplace(t->UniqueID,t);
    for(auto [id,t]:objects) native<<"T "<<id<<' '<<t->GetTechnoType()->ID<<' '
        <<(t->Owner?t->Owner->ArrayIndex:-1)<<' '<<t->Health<<' '<<t->IsAlive<<' '<<t->InLimbo<<' '
        <<t->Location.X<<' '<<t->Location.Y<<' '<<t->Location.Z<<' '<<int(t->CurrentMission)<<' '<<t->Ammo<<' '
        <<t->Cloakable<<' '<<int(t->CloakState)<<' '<<t->CloakProgress.Value<<' '<<t->CloakDelayTimer.GetTimeLeft()<<'\n';
    for(auto foot:FootClass::Array) if(foot) native<<"F "<<foot->UniqueID<<' '<<foot->LastMapCoords.X<<' '
        <<foot->LastMapCoords.Y<<' '<<foot->ThreatPosed<<' '<<foot->IsInAir()<<' '<<foot->GetTargetCoords().Z<<'\n';
    for(int i=0;i<400;++i) for(auto unit:ForceMove::trackerVector(i)) native<<"A "<<i<<' '<<unit->UniqueID<<'\n';
    IStream* stream=nullptr;
    if(FAILED(CreateStreamOnHGlobal(nullptr,TRUE,&stream))) check(false,"snapshot stream");
    // Unrefreshed offer boards are lazy local presentation state. Compare all
    // committed refresh boards and all gameplay state, without making either
    // client generate the remote player's offers as a side effect of sampling.
    const auto boards=Research::savedOffers;
    const auto seen=Research::seenOffers;
    for(auto it=Research::savedOffers.begin();it!=Research::savedOffers.end();) {
        if(!Research::refreshedSlots.contains(it->first)) it=Research::savedOffers.erase(it); else ++it;
    }
    for(auto it=Research::seenOffers.begin();it!=Research::seenOffers.end();) {
        if(!Research::refreshedSlots.contains(it->first)) it=Research::seenOffers.erase(it); else ++it;
    }
    bool saved=saveState(stream);
    Research::savedOffers=boards;Research::seenOffers=seen;
    if(!saved) check(false,"snapshot extension state");
    STATSTG stat{};stream->Stat(&stat,STATFLAG_NONAME);
    std::string extension(stat.cbSize.LowPart,'\0');
    stream->Seek({},STREAM_SEEK_SET,nullptr);stream->Read(extension.data(),stat.cbSize.LowPart,nullptr);stream->Release();
    std::ofstream("snapshots.csv",std::ios::app)<<frame<<','<<std::hex<<hash(native.str())<<','<<hash(extension)<<'\n';
    // Keep the latest full sample for diagnosing a mismatch without hashing pointers.
    std::ofstream("native-last.txt")<<native.str();
    if(frame%10==0) {
        auto name="samples/"+std::to_string(frame);
        std::ofstream(name+".txt")<<native.str();
        std::ofstream(name+".bin",std::ios::binary).write(extension.data(),extension.size());
    }
}
long long state(int role,const char* key) {
    auto it=scriptTimers.state.find({casters[role]->UniqueID,"mp_cast",key});
    return it==scriptTimers.state.end()?0:it->second.integer;
}
void send(const EventClass& event,const char* label) {
    check(EventClass::OutList.Add(event),label);
    std::ofstream("multiplayer.log",std::ios::app)<<"SENT frame="<<Unsorted::CurrentFrame<<" house="<<int(event.HouseIndex)<<" type="<<int(event.Type)<<'\n';
}
void cast(int role,bool duplicate=false) {
    auto h=players[role];
    if(h!=HouseClass::CurrentPlayer) return;
    EventClass event(h->ArrayIndex,EventType::Place,static_cast<AbstractType>(0x53460000u|skillIndex),
        casters[role]->UniqueID,targets[role]->UniqueID,targets[role]->GetMapCoords());
    send(event,"local active command queued");
    if(duplicate) send(event,"duplicate active command queued");
}
void clickCast(int role) {
    if(players[role]!=HouseClass::CurrentPlayer) return;
    ObjectClass::CurrentObjects.Clear();casters[role]->Select();
    auto tactical=TacticalClass::Instance;
    auto world=targets[role]->GetCenterCoords();
    auto screen=tactical->CoordsToScreen(world);
    auto view=DSurface::ViewBounds;
    tactical->TacticalPos={screen.X-view.Width/2,screen.Y-view.Height/2};
    SF_ResearchDraw(nullptr);
    Point2D button{view.X+25,view.Y+view.Height-30};
    check(WindowMouse::button(button,1) && WindowMouse::button(button,4),"real skill button click captured");
    auto point=tactical->CoordsToClient(world).first;
    point.X+=view.X;point.Y+=view.Y;
    auto before=EventClass::OutList.Count;
    check(WindowMouse::button(point,1) && WindowMouse::button(point,4),"real unit target click captured");
    check(EventClass::OutList.Count==before+1 && state(role,"casts")==0,"UI queues exactly one command without local execution");
}
void refresh(int role) {
    auto h=players[role];if(h!=HouseClass::CurrentPlayer) return;
    auto board=Research::offers(h->ArrayIndex,h->Type->ID);
    check(board.size()==3,"local research offers");
    EventClass event(h->ArrayIndex,EventType::SpecialPlace,-0x5347,{0,static_cast<short>(board[0])});
    send(event,"local refresh queued");send(event,"duplicate refresh queued");
}
int selectionMessages(HouseClass* house) {
    auto completed=Research::completedProjects(house->ArrayIndex);
    check(completed.size()==1,"research selected for notification test");
    auto message=std::wstring(house->UIName)+L" 已研究「"+Research::options[completed.front()].name+L"」。";
    int count=0;
    for(const auto& text:MessageListClass::Instance.MessageBuffers) if(message==text) ++count;
    return count;
}
void purchase(int role) {
    auto h=players[role];if(h!=HouseClass::CurrentPlayer) return;
    auto board=Research::offers(h->ArrayIndex,h->Type->ID);
    check(board.size()==3,"refreshed local research offers");
    EventClass event(h->ArrayIndex,EventType::SpecialPlace,-0x5346,{0x5243,static_cast<short>(board[0])});
    send(event,"local purchase queued");send(event,"duplicate purchase queued");
}
void setup() {
    CreateDirectoryA("samples",nullptr);
    Research::ensureReady();
    check(Research::sharedRaritySequence,"shared rarity enabled on both peers");
    for(auto h:HouseClass::Array) if(h && h->IsHumanPlayer && !h->IsObserver()) players.push_back(h);
    check(players.size()==2 && ActiveSkills::specs.size()==1,"network fixture configuration");
    skillIndex=0;
    for(int role=0;role<2;++role) {
        auto h=players[role];credits[role]=h->Available_Money();
        for(auto b:BuildingClass::Array) if(b && b->Owner==h && !_stricmp(b->Type->ID,"GAPOWR")) ++buildingCounts[role];
        for(int kind=0;kind<2;++kind) {
            auto unit=static_cast<TechnoClass*>(UnitTypeClass::Find("MTNK")->CreateObject(h));
            CellStruct cell{static_cast<short>(45+role*7),static_cast<short>(73+kind*3)};
            check(unit && unit->Unlimbo(MapClass::Instance.GetCellAt(cell)->GetCoordsWithBridge(),DirType::North),"place network fixture unit");
            unit->ForceMission(Mission::Sleep);
            (kind?targets:casters)[role]=unit;
        }
        starts[role]=casters[role]->GetMapCoords();health[role]=targets[role]->Health;
        grantToUnit(casters[role],PassiveGrant{{"mp_cast","mp_random"},-1});
        MapClass::Instance.Reveal(h);
    }
    // Only this client's panel is populated. The remote client must independently
    // derive the same board when the serialized refresh/purchase arrives.
    auto local=HouseClass::CurrentPlayer;
    check(Research::offers(local->ArrayIndex,local->Type->ID).size()==3,"asymmetric local UI preparation");
}
}
DEFINE_HOOK(0x55B4EB,SF_TestMultiplayer,5) {
    if(GetFileAttributesA("Starfall.test")==INVALID_FILE_ATTRIBUTES) return 0;
    using namespace MultiplayerTest;
    int frame=Unsorted::CurrentFrame;
    if(frame==20) {
        check(SessionClass::IsMultiplayer() && Game::PlayerCount==2,"real two-player multiplayer session");
        int humans=0;
        for(auto h:HouseClass::Array) if(h && h->IsHumanPlayer && !h->IsObserver()) ++humans;
        check(humans==2,"two human houses");
        std::ofstream("multiplayer.log",std::ios::app)<<"LOCAL house="<<HouseClass::CurrentPlayer->ArrayIndex
            <<" mode="<<int(SessionClass::Instance.GameMode)<<" players="<<Game::PlayerCount<<'\n';
        setup();
    }
    if(frame>20 && frame<=900) for(int role=0;role<2;++role) {
        bool casterFound=false,targetFound=false;
        for(auto unit:TechnoClass::Array) {casterFound|=unit==casters[role];targetFound|=unit==targets[role];}
        if(!casterFound || !targetFound) check(false,"fixture units unexpectedly destroyed");
    }
    if(frame==60) clickCast(0);
    if(frame==120) cast(1);
    if(frame==180) refresh(0);
    if(frame==210) check(Research::slotUsed(players[0]->ArrayIndex,0),"remote refresh applied once on both peers");
    if(frame==220) purchase(0);
    if(frame==240) {cast(0,true);cast(1,true);}
    if(frame==320) {
        for(int role=0;role<2;++role) check(state(role,"casts")==2 && state(role,"pulses")==8,"both players skills and delayed actions executed exactly twice");
        for(int role=0;role<2;++role) {
            int count=0;for(auto b:BuildingClass::Array) if(b && b->Owner==players[role] && !_stricmp(b->Type->ID,"GAPOWR")) ++count;
            check(count==buildingCounts[role]+1,"building fallback executed once per owner on both peers");
            auto id=static_cast<DWORD>(state(role,"buildingId"));
            BuildingClass* building=nullptr;for(auto b:BuildingClass::Array) if(b && b->UniqueID==id) building=b;
            check(building && building->Owner==players[role] && !_stricmp(building->Type->ID,"GAPOWR"),"building conversion keeps script identity on both peers");
        }
        check(Research::unlocked[players[0]->ArrayIndex].size()==1 && Research::unlocked[players[1]->ArrayIndex].empty(),"research purchase owner isolation");
        check(Research::completedRounds.at(players[0]->ArrayIndex)==1 && !Research::completedRounds.contains(players[1]->ArrayIndex),"shared rarity progress is per player");
    }
    if(frame==280) {
        check(Research::announceSelection && selectionMessages(players[0])==1,"one default research announcement on each peer");
        check(WritePrivateProfileStringA("Starfall.Research","AnnounceSelection","no",configPath),"disable announcements in fixture");
        Research::readConfiguration(configPath);
        check(!Research::announceSelection,"announcement switch read");
    }
    if(frame==340) refresh(1);
    if(frame==380) purchase(1);
    if(frame==430) check(selectionMessages(players[1])==0,"disabled research stays silent on each peer");
    if(frame==430) {
        auto first=Research::completedProjects(players[0]->ArrayIndex).front();
        auto second=Research::completedProjects(players[1]->ArrayIndex).front();
        check(Research::options[first].rarity==Research::options[second].rarity,"staggered first purchases share rarity");
        // Sample next-round rarity without populating remote lazy board state.
        const auto boards=Research::savedOffers;
        const auto seen=Research::seenOffers;
        std::array<Research::Rarity,2> rarity{};
        for(int role=0;role<2;++role) {
            auto h=players[role];
            check(Research::completedRounds.at(h->ArrayIndex)==1,"duplicate network purchase advances only once");
            const auto& board=Research::offers(h->ArrayIndex,h->Type->ID);
            check(board.size()==3,"second shared research round available");
            rarity[role]=Research::options[board.front()].rarity;
        }
        check(rarity[0]==rarity[1],"second round shares rarity across players");
        Research::savedOffers=boards;Research::seenOffers=seen;Research::clearOfferCache();
    }
    if(frame==440) {
        auto local=HouseClass::CurrentPlayer;
        int role=local==players[0]?0:1;
        // Legitimate sender with another player's caster must be rejected.
        send(EventClass(local->ArrayIndex,EventType::Place,static_cast<AbstractType>(0x53460000u|skillIndex),
            casters[1-role]->UniqueID,targets[role]->UniqueID,targets[role]->GetMapCoords()),"cross-owner caster rejection command queued");
    }
    if(frame==480) {cast(0);cast(1);}
    if(frame==620) {
        for(int role=0;role<2;++role) {
            check(state(role,"casts")==3 && state(role,"pulses")==12,"duplicate and cross-owner casts rejected");
            check(players[role]->Available_Money()==credits[role]+211,"exactly three credit rewards and one research purchase");
            check(targets[role]->Health<health[role] && targets[role]->Health>0,"delayed damage reached both targets");
            check(Research::unlocked[players[role]->ArrayIndex].size()==1 && Research::balance(players[role]->ArrayIndex)>900,"research ownership and probabilistic income");
            check(!ActiveSkills::remaining(casters[role],ActiveSkills::specs[skillIndex]),"cooldown expired on both peers");
        }
        check(scriptTimers.queue.size()==2 && LuaAnim::entries.empty() && ForceMove::motions.empty(),"only scheduled research rewards remain");
    }
    if(frame==640) for(auto caster:casters) grantToUnit(caster,PassiveGrant{{"mp_cloak"},120});
    if(frame==660) for(auto caster:casters)
        check(cloakableUnits.contains(caster->UniqueID) && caster->IsCloakable() && !caster->Cloakable,"temporary cloak capability on both peers");
    if(frame==680) {
        for(int role=0;role<2;++role) {
            const int house=players[role]->ArrayIndex;
            const auto projects=Research::completedProjects(house);
            check(state(role,"researchRewards")==2 && projects.size()==3 && Research::completedRounds.at(house)==1 &&
                Research::options[projects[1]].rarity==Research::Rarity::Rare,"random research grants synchronized without advancing ordinary rounds");
            check(players[role]->Available_Money()==credits[role]+411,"two random research effects applied once");
        }
        check(scriptTimers.queue.empty(),"timers animations and forced moves retired");
    }
    if(frame==800) for(auto caster:casters)
        check(!cloakableUnits.contains(caster->UniqueID) && !caster->IsCloakable() && !caster->Cloakable,"temporary cloak withdrawn on both peers");
    if(frame==810) {
        BuildingClass* power=nullptr;
        for(auto b:BuildingClass::Array) if(b && b->Owner==players[1] && !_stricmp(b->Type->ID,"GAPOWR")) {power=b;break;}
        auto spy=static_cast<InfantryClass*>(InfantryTypeClass::Find("SPY")->CreateObject(players[0]));
        check(power && spy,"network building reward fixture");
        authorizeBuildingReward(spy,power,Research::BuildingRewardSource::Spy);
        if(HouseClass::CurrentPlayer==players[0]) {
            auto board=Research::buildingOptions(power->UniqueID,players[0]->ArrayIndex);
            check(board.size()==3,"local weighted free board");
            int id=power->UniqueID;
            EventClass claim(players[0]->ArrayIndex,EventType::SpecialPlace,-0x100000-board.back(),
                CellStruct{static_cast<short>(id),static_cast<short>(static_cast<unsigned>(id)>>16)});
            send(claim,"weighted free choice queued");send(claim,"duplicate free choice queued");
        }
    }
    if(frame==810) {
        auto h=players[1];
        check(Research::grantFree(h->ArrayIndex,h->Type->ID,{},Research::Rarity::Rare),"granted one network research choice");
        if(HouseClass::CurrentPlayer==players[0]) {
            const auto id=Research::lastTicket;
            auto board=Research::ticketOptions(id,h->ArrayIndex,h->Type->ID);
            send(EventClass(players[0]->ArrayIndex,EventType::SpecialPlace,-0x110000-board.front(),
                CellStruct{static_cast<short>(id),static_cast<short>(id>>16)}),"other house attempts ticket claim");
        }
    }
    if(frame==845) {
        auto h=players[1];
        check(Research::freeTickets.size()==1 && Research::completedProjects(h->ArrayIndex).size()==3,"other house cannot consume ticket");
        if(HouseClass::CurrentPlayer==h) {
            const auto id=Research::lastTicket;
            const auto& view=DSurface::ViewBounds;
            const Point2D tab{view.X+view.Width-32,view.Y+48};
            SF_ResearchDraw(nullptr);
            check(WindowMouse::button(tab,1) && WindowMouse::button(tab,4) &&
                Research::rewardPanel==(Research::ticketFlag|id),"research tab opens action reward");
            const auto rows=Research::freeOptions(Research::rewardPanel,h->ArrayIndex);
            check(rows.size()==3,"three rare cards in action reward");
            const Point2D middle{view.X+view.Width/2,view.Y+view.Height/2};
            auto before=EventClass::OutList.Count;
            check(WindowMouse::button(middle,1) && WindowMouse::button(middle,4) && EventClass::OutList.Count==before+1,
                "ticket card click queues exactly one command");
            send(EventClass(h->ArrayIndex,EventType::SpecialPlace,-0x110000-rows[1],
                CellStruct{static_cast<short>(id),static_cast<short>(id>>16)}),"repeat ticket claim");
        }
    }
    if(frame==870) {
        check(Research::completedProjects(players[0]->ArrayIndex).size()==4 &&
            Research::completedProjects(players[1]->ArrayIndex).size()==4 && Research::freeTickets.empty() &&
            Research::completedRounds.at(players[1]->ArrayIndex)==1 && Research::completedRounds.at(players[0]->ArrayIndex)==1 &&
            Research::pendingBuildingRewards(players[0]->ArrayIndex).empty(),"weighted building claim commits once on both peers");
        check(players[1]->Available_Money()==credits[1]+511,"ticket click and duplicate grant one project on both peers");
    }
    if(frame==700) for(int role=0;role<2;++role) if(players[role]==HouseClass::CurrentPlayer) {
        auto cell=MapClass::Instance.GetCellAt(CellStruct{static_cast<short>(45+role*7),80});
        send(EventClass(players[role]->ArrayIndex,TargetClass(casters[role]),Mission::Move,TargetClass(),TargetClass(cell),TargetClass()),"native movement queued after skills");
    }
    if(frame==900) for(int role=0;role<2;++role) check(casters[role]->GetMapCoords()!=starts[role],"native movement resumed after skill work");
    if(frame==100 && GetFileAttributesA("diverge.flag")!=INVALID_FILE_ATTRIBUTES) ++targets[0]->Health;
    if(frame>=20 && frame<=900) snapshot(frame);
    if(frame==900) {check(true,"completed network run");TerminateProcess(GetCurrentProcess(),0);}
    return 0;
}
