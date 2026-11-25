namespace GarrisonCheck {
constexpr CellStruct site{70,69}, rifleCell{69,69}, flakCell{71,68}, targetCell{76,69}, outside{77,69};
constexpr CellStruct forceCell{71,67};
DWORD rifle=0, flak=0, ground=0, air=0, ordered=0, neutral=0, civilian=0, insignificant=0;
DWORD nonPassive=0, excludedInfantry=0;
bool done=false;
bool forceFireHeld=false, groundOrdered=false;
int lastRifle=0,lastFlak=0;
int rifleShots=0,flakShots=0;
int rifleReady=0,flakReady=0;

DEFINE_HOOK(0x53EC90,GarrisonCheck_ForceFireKey,5) {
    if(!forceFireHeld || R->ECX<int>()!=(GameOptionsClass::Instance.KeyForceFire1&0xff)) return 0;
    R->EAX(0x8000);
    return 0x53ECA0;
}

void verifyAttackPointer(BuildingClass* post,CellStruct cell,ObjectClass* aim,Action action) {
    MapClass::UnselectAll();
    check(post->Select(),"selected the garrison building for mouse pointer test");
    auto& mouse=MouseClass::Instance;
    check(mouse.DecideAction(cell,aim,true)==action,"map hover offered the garrison attack action");
    mouse.ConvertAction(cell,false,aim,action,false);
    if(mouse.MouseCursorIndex!=MouseCursorType::Attack) std::ofstream("actsp.log",std::ios::app)
        <<"mouse cursor index "<<static_cast<int>(mouse.MouseCursorIndex)<<"\n";
    check(mouse.MouseCursorIndex==MouseCursorType::Attack,"garrison uses stationary attack mouse pointer");
    post->Deselect();
}

void restoreCheckpoint() {
    if(rifle || GetFileAttributesA("reload.flag")==INVALID_FILE_ATTRIBUTES) return;
    std::ifstream checkpoint("garrison-checkpoint.txt");
    check(bool(checkpoint>>rifle>>flak>>ground>>air>>ordered>>neutral>>civilian>>insignificant>>nonPassive>>excludedInfantry
          >>lastRifle>>lastFlak>>rifleShots>>flakShots>>rifleReady>>flakReady),
          "restored garrison test checkpoint");
}

void commit(TechnoClass* firer) {
    restoreCheckpoint();
    const auto id=firer->UniqueID;
    if(!firer->InLimbo || (id!=rifle && id!=flak)) return;
    const bool isRifle=id==rifle;
    const int frame=Unsorted::CurrentFrame;
    const auto cell=abstract_cast<CellClass*>(firer->Target);
    const auto target=firer->Target && !cell?firer->Target->UniqueID:0;
    if(target && target!=ordered && (target==neutral || target==civilian || target==insignificant))
        check(false,"garrison acquired a neutral or insignificant object");
    if(target && (target==nonPassive || target==excludedInfantry))
        check(false,"garrison ignored native weapon targeting");
    if(frame<104) {
        if(target && target==ground) check(false,"garrison respects maximum range");
        return;
    }
    if(isRifle && groundOrdered) {
        check(cell && cell->MapCoords==forceCell,"garrison rifle fired at the ordered ground cell");
    } else {
        check(target==(isRifle?(ordered?ordered:ground):air),isRifle?"garrison rifle targeted its object":"garrison flak targeted air");
    }
    auto& previous=isRifle?lastRifle:lastFlak;
    auto& count=isRifle?rifleShots:flakShots;
    if(previous) {
        const int interval=frame-previous;
        const int expected=isRifle?32:48;
        // The native weapon roll can add up to two frames before the multiplier.
        // The first flak shot after clearing Target also waits for its next scan.
        if(!isRifle && count==1 && flakReady)
            check(frame>=flakReady && frame<flakReady+8,"garrison reacquired after its restored cooldown");
        else
            check(interval>=expected && interval<=expected+2,"garrison individual rearm multiplier");
    }
    previous=frame;
    if(!count++) {
        const auto muzzle=firer->GetFLH(0,firer->GetCoords());
        bool damage=false, port=false;
        for(auto bullet:BulletClass::Array) if(bullet->Owner==firer && bullet->Health==(isRifle?48:144)) {
            damage=true;
            port=bullet->SourceCoords==muzzle;
        }
        check(damage,"default 20 percent garrison damage applied to occupant projectile");
        check(port,"occupant projectile launched from the assigned firing port");
    }
}

InfantryClass* recruit(const char* type,CellStruct cell,BuildingClass* post,HouseClass* house) {
    auto unit=static_cast<InfantryClass*>(InfantryTypeClass::Find(type)->CreateObject(house));
    check(unit && unit->Unlimbo(MapClass::Instance.GetCellAt(cell)->GetCoords(),DirType::North),"placed garrison recruit");
    check(post->CanBeOccupiedBy(unit),"recruit can occupy fixture");
    return unit;
}

void tick(int frame,bool load,HouseClass* house) {
    restoreCheckpoint();
    if(frame==5 && !load) placeBuilding("NABNKR",site,house);
    if(frame==25 && !load) {
        auto post=outpost(site);
        check(post && post->Type->CanBeOccupied && post->Type->CanOccupyFire,"native garrison building rules");
        auto unit=recruit("E1",rifleCell,post,house);
        const auto action=unit->MouseOverObject(post,true);
        check(action==Action::Capture,"native garrison cursor");
        check(unit->ObjectClickedAction(action,post,false),"issued native garrison order");
    }
    static bool sentFlak=false;
    if(!load && !sentFlak && frame>=30 && frame<90) {
        auto post=outpost(site);
        if(post && post->Occupants.Count==1) {
            auto unit=recruit("FLAKT",flakCell,post,house);
            // Finish the second arrival using the engine's garrison-entry routine.
            using EnterBuilding=void (__thiscall*)(InfantryClass*,BuildingClass*);
            reinterpret_cast<EnterBuilding>(0x522910)(unit,post);
            sentFlak=true;
        }
    }
    if(frame<90 || done) return;
    auto post=outpost(site);
    if(frame==90) {
        check(post && post->Occupants.Count==2,"native mixed garrison entered");
        check(post->Passengers.NumPassengers==0,"garrison uses native occupant list");
        check(!post->CanOccupyFire(),"legacy garrison fire intercepted");
        for(auto unit:post->Occupants) {
            if(!_stricmp(unit->Type->ID,"E1")) rifle=unit->UniqueID;
            if(!_stricmp(unit->Type->ID,"FLAKT")) {
                flak=unit->UniqueID;
                check(grantToUnit(unit,PassiveGrant{{"garrisoncombat"},-1}),"granted garrisoned unit a combat passive");
            }
        }
        check(rifle && flak,"mixed garrison identity");
        check(post->Occupants[0]->UniqueID==rifle,"rifle entered the garrison first");
        HouseClass* foe=nullptr;
        for(auto h:HouseClass::Array) if(h && !h->IsNeutral() && !h->Defeated && !house->IsAlliedWith(h)) {foe=h;break;}
        check(foe!=nullptr,"garrison enemy");
        auto passiveHouse=HouseClass::FindNeutral();
        check(passiveHouse && passiveHouse->IsNeutral(),"neutral house for garrison decoys");
        auto power=placeBuilding("GAPOWR",{72,66},passiveHouse);
        neutral=power->UniqueID;
        auto civilianUnit=place("TRUCKA",{68,67},passiveHouse);
        civilianUnit->Deactivate();
        civilian=civilianUnit->UniqueID;
        auto ignoredEnemy=place("TRUCKB",{68,68},foe);
        check(ignoredEnemy->Type->Insignificant,"enemy decoy excluded from passive targeting");
        ignoredEnemy->Deactivate();
        insignificant=ignoredEnemy->UniqueID;
        check(post->MouseOverObject(power,true)==Action::Attack,"normal manual attack can select a neutral building");
        check(post->MouseOverObject(civilianUnit,true)==Action::Attack,"normal manual attack can select a neutral vehicle");
        auto passiveDecoy=place("SREF",{69,67},foe);
        passiveDecoy->Deactivate();
        nonPassive=passiveDecoy->UniqueID;
        check(passiveDecoy->Type->Armor==Armor::Special_2 && post->MouseOverObject(passiveDecoy,true)==Action::Attack,
              "low-verses armor is available to manual orders");
        auto infantry=static_cast<InfantryClass*>(InfantryTypeClass::Find("E2")->CreateObject(foe));
        check(infantry && infantry->Unlimbo(MapClass::Instance.GetCellAt(CellStruct{71,66})->GetCoords(),DirType::North),
              "placed hostile infantry excluded by the weapon");
        infantry->Deactivate();
        excludedInfantry=infantry->UniqueID;
        check(post->MouseOverObject(infantry,true)!=Action::Attack,"manual order respects Phobos CanTarget");
        // Also exercise a stale Target restored or left by an earlier order.
        for(auto unit:post->Occupants) if(unit->UniqueID==rifle) {
            unit->SetTarget(passiveDecoy);
            unit->RearmTimer.Stop();
        }
        auto target=place("MTNK",outside,foe);
        target->Deactivate();
        ground=target->UniqueID;
        check(post->MouseOverObject(target,true)==Action::NoMove,"out-of-range object offers no attack action");
    }
    if(frame==104) {
        check(!rifleShots && !flakShots,"garrison respects maximum range");
        for(auto unit:post->Occupants) {
            auto target=unit->Target?unit->Target->UniqueID:0;
            check(target!=neutral && target!=civilian && target!=insignificant,
                  "garrison ignored neutral buildings, civilian vehicles and insignificant enemies");
            check(target!=nonPassive && target!=excludedInfantry,"garrison respected native passive acquire and CanTarget");
        }
        for(auto unit:post->Occupants) {
            unit->SetTarget(nullptr);
            unit->RearmTimer.Stop();
        }
        for(auto unit:UnitClass::Array) if(unit->UniqueID==ground) {
            auto loc=MapClass::Instance.GetCellAt(targetCell)->GetCoords();
            loc.X-=96; // Just under six cells, past the five-cell global range.
            check(ForceMove::moveTo(unit,loc.X,loc.Y,true),"garrison target moved to building type bonus range");
            check(post->MouseOverObject(unit,true)==Action::Attack,"building type range override applied to garrison cursor");
            break;
        }
        HouseClass* foe=nullptr;
        for(auto h:HouseClass::Array) if(h && !h->IsNeutral() && !h->Defeated && !house->IsAlliedWith(h)) {foe=h;break;}
        auto plane=static_cast<AircraftClass*>(AircraftTypeClass::Find("ORCA")->CreateObject(foe));
        auto pos=MapClass::Instance.GetCellAt(CellStruct{69,65})->GetCoords();
        pos.Z+=256;
        check(plane && plane->Unlimbo(pos,DirType::North),"placed garrison aircraft target");
        plane->SetHeight(256);
        plane->Deactivate();
        air=plane->UniqueID;
    }
    if(frame==115) {
        check(rifleShots==1 && flakShots==1,"both garrison weapons began rearming before save");
        HouseClass* foe=nullptr;
        for(auto unit:UnitClass::Array) if(unit->UniqueID==ground) {foe=unit->Owner;break;}
        check(foe!=nullptr,"original ground target still active");
        for(auto aircraft:AircraftClass::Array) if(aircraft->UniqueID==air) {
            check(post->MouseOverObject(aircraft,true)!=Action::Attack,"second occupant cannot add manual anti-air targeting");
            post->ObjectClickedAction(Action::Attack,aircraft,false); // Simulate an incompatible synchronized order.
            break;
        }
        BuildingClass* target=nullptr;
        for(auto building:BuildingClass::Array) if(building->UniqueID==neutral) {target=building;break;}
        check(target!=nullptr,"neutral building still active");
        const auto action=post->MouseOverObject(target,true);
        check(action==Action::Attack,"garrison building offers attack cursor via occupant weapon");
        verifyAttackPointer(post,target->GetMapCoords(),target,action);
        check(post->ObjectClickedAction(action,target,false),"submitted garrison focus order");
        ordered=target->UniqueID;
    }
    if(frame==119) {
        for(auto unit:post->Occupants) {
            auto& due=unit->UniqueID==rifle?rifleReady:flakReady;
            due=frame+unit->RearmTimer.GetTimeLeft();
            check(due>frame,"garrison rearm was in progress at save");
            unit->SetTarget(nullptr);
        }
        std::ofstream("garrison-checkpoint.txt")<<rifle<<" "<<flak<<" "<<ground<<" "<<air<<" "<<ordered<<" "
            <<neutral<<" "<<civilian<<" "<<insignificant<<" "<<nonPassive<<" "<<excludedInfantry<<" "
            <<lastRifle<<" "<<lastFlak<<" "<<rifleShots<<" "<<flakShots<<" "<<rifleReady<<" "<<flakReady;
    }
    if(frame==125) {
        check(post && post->Occupants.Count==2,"native mixed garrison survived save");
        for(auto unit:post->Occupants) {
            const auto id=unit->UniqueID;
            const auto due=id==rifle?rifleReady:flakReady;
            check((id==rifle || id==flak) && unit->RearmTimer.GetTimeLeft()==due-frame,"garrison rearm countdown survived save");
        }
    }
    if(frame==170) {
        check(rifleShots>=2 && flakShots>=2,"both garrison occupants fired and rearmed independently");
        auto cell=forceCell;
        check(post->MouseOverCell(&cell)!=Action::Attack,"a normal terrain hover does not force fire");
        forceFireHeld=true;
        check(WWKeyboardClass::Instance->IsForceFireKeyPressed(),"force-fire modifier held");
        check(post->MouseOverCell(&cell,false,true)!=Action::Attack,"cell action respects ignoreForce");
        check(post->MouseOverCell(&outside)==Action::NoMove,"out-of-range ground offers no force-fire action");
        const auto action=post->MouseOverCell(&cell);
        check(action==Action::Attack,"Ctrl shows an attack action on empty ground");
        verifyAttackPointer(post,cell,nullptr,action);
        check(post->CellClickedAction(action,&cell,&cell,false),"submitted native force-fire cell click");
        forceFireHeld=false;
        groundOrdered=true;
    }
    if(frame==210) {
        check(rifleShots>=4,"ground order persisted for repeated shots");
        check(post->ObjectClickedAction(Action::Self_Deploy,post,false),"submitted native garrison unload");
    }
    if(frame==245) {
        check(post && post->Occupants.Count==0,"native garrison unload survived firing");
        int released=0;
        for(auto unit:InfantryClass::Array) if((unit->UniqueID==rifle || unit->UniqueID==flak) && unit->IsOnMap &&
            !unit->InLimbo && !unit->Transporter) ++released;
        check(released==2,"both original garrison soldiers returned to map");
        // Remove the probe's units and targets before the longer aircraft tests finish.
        for(auto id:{rifle,flak,ground,air,ordered,neutral,civilian,insignificant,nonPassive,excludedInfantry}) {
            for(auto unit:TechnoClass::Array) if(unit->UniqueID==id) {
                unit->UnInit();
                break;
            }
        }
        rifle=flak=0;
        post->UnInit();
        done=true;
    }
}
}

namespace ExcavatorCheck {
bool tick(int frame,HouseClass* house) {
    static BuildingClass* post=nullptr;
    static UnitClass* excavator=nullptr;
    static int health=0, oldTransportSlot=0;
    static int damage=0, expectedInterval=0, firstHit=0, hits=0;
    if(frame==1) {
        post=placeBuilding("CANEWY06",{63,66},HouseClass::FindNeutral());
        auto recruit=GarrisonCheck::recruit("E1",{62,66},post,house);
        oldTransportSlot=recruit->Type->OpenTransportWeapon;
        recruit->Type->OpenTransportWeapon=1;
        using EnterBuilding=void (__thiscall*)(InfantryClass*,BuildingClass*);
        reinterpret_cast<EnterBuilding>(0x522910)(recruit,post);
        excavator=place("CONA",{67,66},HouseClass::FindNeutral());
        excavator->Deactivate();
        health=excavator->Health;
        auto weapon=recruit->GetWeapon(1)->WeaponType;
        damage=MapClass::GetTotalDamage(weapon->Damage*2,weapon->Warhead,excavator->Type->Armor,0);
        expectedInterval=weapon->ROF*2;
    }
    if(frame==6) {
        check(post && post->Occupants.Count==1,"GI inside the Wall Street Office");
        const auto action=post->MouseOverObject(excavator,true);
        check(action==Action::Attack,"GI can point at a neutral excavator from the tall building");
        GarrisonCheck::verifyAttackPointer(post,excavator->GetMapCoords(),excavator,action);
        check(post->ObjectClickedAction(action,excavator,false),"issued the neutral excavator attack through the building");
    }
    if(frame>6 && excavator->Health<health) {
        check(health-excavator->Health==damage,"building type damage override applied to garrison shot");
        health=excavator->Health;
        if(hits++==0) firstHit=frame;
        else if(hits==2) {
            const int interval=frame-firstHit;
            check(interval>=expectedInterval && interval<=expectedInterval+4,"building type fire-rate override applied to garrison reload");
        }
    }
    if(frame==50) {
        check(hits>=2,"GI hits the neutral excavator from the tall building");
        const auto distant=MapClass::Instance.GetCellAt(CellStruct{70,66})->GetCoords();
        check(ForceMove::moveTo(excavator,distant.X,distant.Y,true),"moved neutral excavator out of range");
        check(post->MouseOverObject(excavator,true)==Action::NoMove,"tall building shows a blocked action for the distant excavator");
        auto rifle=post->Occupants[0];
        rifle->Type->OpenTransportWeapon=oldTransportSlot;
        post->UnInit();
        excavator->UnInit();
        return true;
    }
    return false;
}
}
