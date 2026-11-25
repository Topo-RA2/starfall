#include "../../../src/Starfall.cpp"
#include <EventClass.h>
namespace {
DWORD orderTank=0,orderTarget=0;
int unitShots=0,groundShots=0;
void check(bool ok,const char* name) {
    std::ofstream("lua-skills.log",std::ios::app)<<(ok?"PASS ":"FAIL ")<<name<<"\n";
    if(!ok) TerminateProcess(GetCurrentProcess(),2);
}
template<class T> T* place(TechnoTypeClass* type,HouseClass* house,short x,short y) {
    auto u=static_cast<T*>(type->CreateObject(house));
    auto cell=MapClass::Instance.TryGetCellAt(CellStruct{x,y});
    check(u && cell && u->Unlimbo(cell->GetCoordsWithBridge(),DirType::North),"place unit");
    if(u->WhatAmI()==AbstractType::Unit) u->IronCurtain(300,house,false);
    return u;
}
}
DEFINE_HOOK(0x6FF66C,SF_TestLuaOrderShot,6) {
    auto firer=R->ESI<TechnoClass*>();
    if(orderTank && firer->UniqueID==orderTank && firer->Target) {
        if(firer->Target->UniqueID==orderTarget) ++unitShots;
        if(firer->Target==MapClass::Instance.GetCellAt(CellStruct{55,70})) ++groundShots;
    }
    return 0;
}
DEFINE_HOOK(0x55B4EB,SF_TestLuaSkills,5) {
    if(GetFileAttributesA("Starfall.test")==INVALID_FILE_ATTRIBUTES) return 0;
    const bool loading=GetFileAttributesA("reload.flag")!=INVALID_FILE_ATTRIBUTES;
    static InfantryClass* gi=nullptr;
    static UnitClass *nearTank=nullptr,*farTank=nullptr,*enemy=nullptr,*second=nullptr;
    static int start=0;
    auto frame=Unsorted::CurrentFrame;
    auto human=HouseClass::FindByPlayerAt(HouseClass::PlayerAtA);
    auto foe=HouseClass::FindByPlayerAt(HouseClass::PlayerAtB);
    if(!loading && frame==20) {
        auto infantry=InfantryTypeClass::Find("E1");auto tank=UnitTypeClass::Find("MTNK");
        gi=place<InfantryClass>(infantry,human,48,70);
        nearTank=place<UnitClass>(tank,human,49,70);second=place<UnitClass>(tank,human,48,72);
        farTank=place<UnitClass>(tank,human,60,75);enemy=place<UnitClass>(tank,foe,49,71);enemy->Deactivate();
        check(ownsPassive(gi,"sf_fieldcommander") && skills.contains("eatscript") &&
              !skills.contains("missingfunction"),"registration");
        grantToUnit(nearTank,PassiveGrant{{"promotionreward"},-1});
        start=Research::balance(human->ArrayIndex);
    }
    if(!loading && frame==24) {
        grantToUnit(gi,PassiveGrant{{"orderscript"},-1});tickGains();dispatch();
    }
    if(!loading && frame==80) {
        std::ofstream("lua-skills.log",std::ios::app)<<"deployed "<<gi->IsDeployed()<<" rank near/second/gi/far/enemy "
            <<nearTank->Veterancy.Veterancy<<" "<<second->Veterancy.Veterancy<<" "<<gi->Veterancy.Veterancy<<" "
            <<farTank->Veterancy.Veterancy<<" "<<enemy->Veterancy.Veterancy<<"\n";
        check(nearTank->Veterancy.IsVeteran() && second->Veterancy.IsVeteran() && gi->Veterancy.IsRookie() &&
              farTank->Veterancy.IsRookie() && enemy->Veterancy.IsRookie(),"deploy promotion and filters");
        check(Research::balance(human->ArrayIndex)==start+7,"promotion event");
        grantToUnit(nearTank,PassiveGrant{{"damagescript","brokenscript","eatscript","interceptedorder"},-1});
        auto& params=skills.at("damagescript").scriptParams;
        params["output"]=std::to_string(human->PowerOutput);
        params["drain"]=std::to_string(human->PowerDrain);
        params["credits"]=std::to_string(human->Available_Money());
        params["points"]=std::to_string(Research::balance(human->ArrayIndex));
        params["enemycredits"]=std::to_string(foe->Available_Money());
        params["enemypoints"]=std::to_string(Research::balance(foe->ArrayIndex));
        check(adjustPendingDamage("onbeforedealdamage",nearTank,enemy,nearTank,enemy,31)==15,"synchronous damage");
        capture("ontakedamage",nearTank,enemy,enemy,nearTank,1);dispatch();
        std::ifstream log("Starfall.log");string errors((std::istreambuf_iterator<char>(log)),{});
        check(errors.find("deliberate fixture error")!=string::npos && errors.find("Checks.lua:")!=string::npos,"error isolation");
        check(errors.find("unit orders unavailable during damage interception")!=string::npos,"damage interception rejects unit orders");
        auto cargo=static_cast<FootClass*>(InfantryTypeClass::Find("E1")->CreateObject(human));
        nearTank->EnterAsPassenger(cargo);cargo->Transporter=nearTank;
        grantToUnit(cargo,PassiveGrant{{"cargobuff"},-1});changeStacks(cargo->UniqueID,"cargobuff",3,INT_MAX);
        capture("onfire",nearTank,enemy,nearTank,enemy,0);dispatch();
        check(nearTank->Passengers.NumPassengers==1 && !ownsPassive(nearTank,"cargobuff"),"INI count");
        auto scout=place<UnitClass>(UnitTypeClass::Find("MTNK"),human,59,69);
        auto dummy=place<UnitClass>(UnitTypeClass::Find("HTK"),foe,54,71);
        dummy->IronCurtainTimer.Stop();dummy->Health=10000;dummy->Deactivate();
        grantToUnit(scout,PassiveGrant{{"orderscript"},-1});tickGains();dispatch();
        check(scout->Destination==MapClass::Instance.GetCellAt(CellStruct{56,69}),"Lua movement destination accepted");
        capture("onfire",nearTank,enemy,nearTank,enemy,0);
        std::ofstream("lua-skills.id")<<nearTank->UniqueID<<" "<<Research::balance(human->ArrayIndex)<<" "<<scout->UniqueID<<" "<<dummy->UniqueID;
        check(ScenarioClass::SaveGame("Saved Games\\LuaSkills.sav",L"Lua Skills"),"saved");
        TerminateProcess(GetCurrentProcess(),0);
    }
    if(loading && frame==85) {
        DWORD id;int balance;std::ifstream("lua-skills.id")>>id>>balance>>orderTank>>orderTarget;
        for(auto unit:UnitClass::Array) if(unit->UniqueID==id) nearTank=unit;
        check(nearTank && nearTank->Passengers.NumPassengers==0 && stackCount(id,"cargobuff")==2 &&
              Research::balance(human->ArrayIndex)==balance+3,"loaded action chain");
        check(nearTank->IsIronCurtained() && nearTank->IronCurtainTimer.GetTimeLeft()>590,"Lua iron curtain");
        dispatch();check(Research::balance(human->ArrayIndex)==balance+3,"no duplicate callback");
    }
    if(loading && frame==330) {
        const auto& state=scriptTimers.state;
        check(state.contains({orderTank,"orderscript","attackIssued"}) && unitShots>0,"Lua unit reached destination and fired at ordered unit after load");
        std::ofstream("lua-skills.log",std::ios::app)<<"ordered shots: unit="<<unitShots<<" ground="<<groundShots<<'\n';
        check(state.contains({orderTank,"orderscript","groundIssued"}) && groundShots>0,"Lua unit fired at ordered ground cell");
        TerminateProcess(GetCurrentProcess(),0);
    }
    return 0;
}
