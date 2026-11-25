bool missileImpact=false,convertedInFlight=false,replenished=false;
DWORD carrierId=0,droneId=0,launcherId=0,missileId=0,excessId=0,targetId=0;

UnitClass* carrierFixture(UnitTypeClass* type,HouseClass* owner,CellStruct cell) {
    auto unit=static_cast<UnitClass*>(type->CreateObject(owner));
    check(unit && unit->Unlimbo(MapClass::Instance.GetCellAt(cell)->GetCoords(),DirType::North),"placed conversion carrier fixture");
    return unit;
}
UnitClass* carrierById(DWORD id) {
    for(auto unit:UnitClass::Array) if(unit->UniqueID==id) return unit;
    return nullptr;
}
void setupCarriers(HouseClass* house) {
    auto plain=UnitTypeClass::Find("MTNK"),singleSlot=UnitTypeClass::Find("TRUCKA");
    auto dualSlot=UnitTypeClass::Find("TRUCKB"),launcherType=UnitTypeClass::Find("TNKD");
    auto drone=AircraftTypeClass::Find("SFConvDrone"),missile=AircraftTypeClass::Find("SFConvMissile");
    check(drone && missile && singleSlot->Spawns==drone && dualSlot->Spawns==drone && launcherType->Spawns==missile,"Ares custom spawn types registered");
    auto researchCarrier=carrierFixture(singleSlot,house,{63,82});
    auto researchSlot=researchCarrier->SpawnManager->SpawnedNodes[0]->Unit;
    check(Research::available(1),"carrier research eligible");
    Research::award(house->ArrayIndex,100);
    check(Research::purchase(house->ArrayIndex,1,house->Type->ID) && researchCarrier->Type==dualSlot &&
        researchCarrier->SpawnManager && researchCarrier->SpawnManager->SpawnCount==2 &&
        researchCarrier->SpawnManager->SpawnedNodes[0]->Unit==researchSlot && researchSlot->SpawnOwner==researchCarrier,
        "research converted carrier with its original drone");
    auto carrier=carrierFixture(plain,house,{82,72});
    check(transformUnit(carrier,dualSlot,unimplementedConversion),"gain custom drone carrier");
    auto manager=carrier->SpawnManager;
    check(manager && manager->Owner==carrier && manager->SpawnCount==2,"created carrier slots");
    auto first=manager->SpawnedNodes[0]->Unit;
    auto excess=manager->SpawnedNodes[1]->Unit;
    check(first && excess && first->Type==drone && first->SpawnOwner==carrier,"custom drone attached");
    excessId=excess->UniqueID;
    check(transformUnit(carrier,singleSlot,unimplementedConversion) && carrier->SpawnManager==manager &&
        manager->SpawnCount==1 && manager->SpawnedNodes[0]->Unit==first,"retained occupied slot on shrink");
    auto launcher=carrierFixture(dualSlot,house,{80,76});
    check(transformUnit(launcher,plain,unimplementedConversion) && !launcher->SpawnManager,"removed carrier capability");
    check(transformUnit(launcher,singleSlot,unimplementedConversion) && transformUnit(launcher,launcherType,unimplementedConversion),"changed drone to custom missile");
    auto rocket=launcher->SpawnManager->SpawnedNodes[0];
    check(rocket->IsSpawnMissile && rocket->Unit && rocket->Unit->Type==missile && rocket->Unit->SpawnOwner==launcher,"Ares classified custom missile");
    carrierId=carrier->UniqueID;droneId=first->UniqueID;launcherId=launcher->UniqueID;missileId=rocket->Unit->UniqueID;
    auto foe=HouseClass::FindByPlayerAt(HouseClass::PlayerAtB);
    check(foe && foe!=house,"carrier target house");
    auto target=carrierFixture(UnitTypeClass::Find("MTNK"),foe,{84,76});
    target->Health=target->EstimatedHealth=2000;
    target->ForceMission(Mission::Sleep);
    targetId=target->UniqueID;
    std::ofstream("conversion-carriers.id")<<carrierId<<' '<<droneId<<' '<<launcherId<<' '<<missileId<<' '<<excessId<<' '<<targetId;
}
void checkCarrierFlight(int frame,bool loading) {
    if(!carrierId) std::ifstream("conversion-carriers.id")>>carrierId>>droneId>>launcherId>>missileId>>excessId>>targetId;
    auto carrier=carrierById(carrierId),launcher=carrierById(launcherId);
    if(!carrier || !launcher || !carrier->SpawnManager || !launcher->SpawnManager) {
        check(false,"restored carriers");return;
    }
    auto manager=carrier->SpawnManager;
    auto slot=manager->SpawnedNodes[0];
    if(loading && !convertedInFlight) {
        check(carrier->Type==UnitTypeClass::Find("TRUCKB") && manager->SpawnCount==2 &&
            slot->Unit && slot->Unit->UniqueID==droneId && slot->Unit->SpawnOwner==carrier &&
            !slot->Unit->InLimbo && manager->SpawnedNodes[1]->Status==SpawnNodeStatus::Dead &&
            manager->SpawnedNodes[1]->SpawnTimer.GetTimeLeft()>0,"loaded airborne drone and pending regeneration");
        convertedInFlight=true;
    }
    if(frame==30 && !loading) {
        check(std::none_of(AircraftClass::Array.begin(),AircraftClass::Array.end(),
            [](AircraftClass* air){return air->UniqueID==excessId;}),"removed surplus child");
        auto target=carrierById(targetId);
        check(target!=nullptr,"carrier firing target");
        for(auto unit:{carrier,launcher}) {
            unit->SetTarget(target);
            unit->ForceMission(Mission::Attack);
            unit->SpawnManager->SetTarget(target);
        }
    }
    if(frame>=30 && !convertedInFlight && slot->Unit && slot->Unit->IsOnMap && !slot->Unit->InLimbo) {
        auto child=slot->Unit;
        auto status=slot->Status;
        auto aim=manager->Target;
        check(child->UniqueID==droneId && transformUnit(carrier,UnitTypeClass::Find("TRUCKB"),unimplementedConversion) &&
            carrier->SpawnManager==manager && manager->SpawnedNodes[0]==slot && slot->Unit==child &&
            child->SpawnOwner==carrier && slot->Status==status && manager->Target==aim,"airborne custom drone survives conversion");
        check(manager->RegenRate==24 && manager->ReloadRate==36 && manager->SpawnCount==2 &&
            manager->SpawnedNodes[1]->Status==SpawnNodeStatus::Dead && !manager->SpawnedNodes[1]->Unit,"extra slot starts on new regeneration timer");
        convertedInFlight=true;
        check(ScenarioClass::SaveGame("Saved Games\\Conversion.sav",L"Conversion"),"saved");
    }
    if(convertedInFlight) {
        auto next=manager->SpawnedNodes[1]->Unit;
        if(next && next->Type==manager->SpawnType && next->SpawnOwner==carrier) replenished=true;
    }
    auto node=launcher->SpawnManager->SpawnedNodes[0];
    const bool rearmed=node->IsSpawnMissile && node->Unit && node->Unit->UniqueID!=missileId &&
        node->Unit->Type==launcher->Type->Spawns && node->Unit->SpawnOwner==launcher;
    if((convertedInFlight && replenished && missileImpact && rearmed && frame>30) || frame==750) {
        check(convertedInFlight && replenished,"custom drone regenerated");
        check(missileImpact && rearmed,"custom missile hit with configured damage and regenerated");
        TerminateProcess(GetCurrentProcess(),0);
    }
}
