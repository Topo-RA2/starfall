#include "../../../src/Starfall.cpp"

namespace {
DWORD shooter=0;
std::vector<int> frames;
void expect(bool ok,const char* message) {
    std::ofstream("examples.log",std::ios::app)<<(ok?"PASS ":"FAIL ")<<message<<"\n";
    if(!ok) TerminateProcess(GetCurrentProcess(),2);
}
}
DEFINE_HOOK(0x55B4EB,SF_ExampleStarters,5) {
    const int frame=Unsorted::CurrentFrame;
    if(frame==22) {
        Research::ensureReady();
        auto player=HouseClass::FindByPlayerAt(HouseClass::PlayerAtA);
        UnitClass* tank=nullptr;
        for(auto unit:UnitClass::Array) if(unit->Owner==player && !_stricmp(unit->Type->ID,"MTNK")) {tank=unit;break;}
        expect(tank!=nullptr,"placed tank");
        shooter=tank->UniqueID;
        auto point=tank->GetMapCoords();point.Y+=3;
        auto rival=HouseClass::FindByPlayerAt(HouseClass::PlayerAtB);
        auto dummy=static_cast<UnitClass*>(tank->Type->CreateObject(rival));
        expect(dummy && dummy->Unlimbo(MapClass::Instance.GetCellAt(point)->GetCoordsWithBridge(),DirType::North),"placed target");
        dummy->Deactivate();
        tank->Health=100;
        capture("onkill",tank,dummy,tank,dummy,0);dispatch();
        expect(tank->Health==150,"repair on kill");
        const int h=player->ArrayIndex,credits=player->Available_Money();
        const auto choices=Research::offers(h,player->Type->ID);
        auto cash=std::find_if(choices.begin(),choices.end(),[](int i){return Research::options[i].id=="demo_reservecash";});
        expect(Research::balance(h)==100 && choices.size()==3 && cash!=choices.end() &&
            Research::purchase(h,*cash,player->Type->ID) && player->Available_Money()==credits+500 && !Research::balance(h),"three choices and cash award");
        auto facing=tank->GetTargetDirection(dummy);
        tank->PrimaryFacing.SetCurrent(facing);tank->SecondaryFacing.SetCurrent(facing);
        tank->RearmTimer.Stop();tank->Fire(dummy,0);dispatch();
    }
    if(frame>=22 && frame<=30) {
        static std::set<DWORD> seen;
        for(auto bullet:BulletClass::Array) if(bullet && bullet->Owner && bullet->Owner->UniqueID==shooter &&
            passiveBullets.contains(bullet->UniqueID) && seen.insert(bullet->UniqueID).second) frames.push_back(frame);
        if(frame==30) {
            expect(frames.size()==2 && frames[0]==22 && frames[1]-frames[0]==6,"two scripted shots six frames apart");
            TerminateProcess(GetCurrentProcess(),0);
        }
    }
    return 0;
}
