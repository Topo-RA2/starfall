namespace {
constexpr auto exposed="sf_hx_armorpiercing_exposed";
DWORD markedId=0,ggiId=0,flakId=0,chargeTankId=0;
std::vector<DWORD> armorPiercingCasualties;

int sampleDamage(TechnoClass* target,TechnoClass* source) {
    int amount=20,before=target->Health;
    target->ReceiveDamage(&amount,0,WarheadTypeClass::Find("AP"),source,false,false,source->Owner);
    settle();
    return before-target->Health;
}
int nativeRound(TechnoClass* unit,TechnoClass* target,int slot) {
    unit->SetTarget(target);unit->RearmTimer.Stop();unit->CurrentBurstIndex=0;
    auto facing=unit->GetTargetDirection(target);
    unit->PrimaryFacing.SetCurrent(facing);unit->SecondaryFacing.SetCurrent(facing);
    unit->CurrentGattlingStage=slot/2;
    tickStates();
    const int selected=unit->SelectWeapon(target);
    check(selected==slot,"native stage selects corresponding ground or air slot");
    auto shot=unit->Fire(target,selected);
    check(shot!=nullptr,"native gattling round created");
    int damage=shot->Health;
    shot->UnInit();unit->SetTarget(nullptr);
    return damage;
}
void checkFireSupport(int frame,bool loading,HouseClass* owner,HouseClass* foe) {
    if(!loading && frame==90) {
        auto tank=place("YTNK",owner,{60,90});
        auto ground=place("MTNK",foe,{61,90});
        auto air=place("ORCA",foe,{61,91});air->SetHeight(512);
        auto trooper=place("SHK",owner);
        auto ggi=place("GGI",owner),flak=place("FLAKT",owner);
        const int range=trooper->GetWeaponRange(0);
        std::array<TechnoClass*,2> towers={place("YAGGUN",owner),place("SF_HX_GiantYAGGUN",owner)};
        settle(); // The giant has its own independent firepower bonus.
        std::array<std::array<int,2>,2> towerDamage{};
        for(size_t i=0;i<towers.size();++i) {
            towerDamage[i]={nativeRound(towers[i],ground,4),nativeRound(towers[i],air,5)};
        }
        std::array<int,6> baseline{};
        for(int slot=0;slot<6;++slot) baseline[slot]=nativeRound(tank,slot%2?air:ground,slot);
        auto elite=place("YTNK",owner,{59,90});elite->Veterancy.SetElite();
        const int eliteGround=nativeRound(elite,ground,4),eliteAir=nativeRound(elite,air,5);
        unlock(owner,"sf_hx_gatlingdownpour",Research::Rarity::Rare);
        unlock(owner,"sf_hx_armorpiercing",Research::Rarity::Rare);
        unlock(owner,"sf_hx_smokeshell",Research::Rarity::Rare);
        unlock(owner,"sf_hx_teslalunge",Research::Rarity::Common);
        for(int slot:{0,1,2,3,4,5,2}) {
            const int value=nativeRound(tank,slot%2?air:ground,slot);
            std::ofstream("hex-tactics.log",std::ios::app)<<"gattling slot "<<slot<<" damage "<<baseline[slot]<<" -> "<<value<<'\n';
            check(baseline[slot]>0 && value==(slot>=4?std::lround(baseline[slot]*1.8):baseline[slot]),
                "ground and air rounds follow current Gattling stage");
        }
        check(eliteGround>0 && eliteAir>0 &&
            std::abs(nativeRound(elite,ground,4)-eliteGround*1.8)<=2 &&
            std::abs(nativeRound(elite,air,5)-eliteAir*1.8)<=2,"elite final stage rounds gain damage");
        for(size_t i=0;i<towers.size();++i) {
            for(int slot:{4,5}) {
                int boosted=nativeRound(towers[i],slot==4?ground:air,slot);
                check(boosted==std::lround(towerDamage[i][slot-4]*1.8),
                    "ordinary and giant gattling cannons fire boosted final stage rounds");
            }
        }
        for(auto id:{"HTK","NAFLAK","SF_HX_GiantNAFLAK"}) {
            auto u=place(id,owner);check(ownsPassive(u,"sf_hx_smokeshell_hit"),"all flak sources receive smoke");
        }
        check(std::abs(trooper->FirepowerMultiplier-1.75)<1e-5 &&
            std::abs(static_cast<FootClass*>(trooper)->SpeedMultiplier-1.30)<1e-5 &&
            std::abs(trooper->GetWeaponRange(0)-range*.70)<=1,"Tesla trooper range speed and damage");
        const int first=sampleDamage(ground,ggi);
        const int follow=sampleDamage(ground,flak);
        check(first>0 && follow==int(first*1.3) && ownsPassive(ground,exposed),"only subsequent hits gain thirty percent on marked vehicle");
        sampleDamage(air,ggi);check(ownsPassive(air,exposed),"aircraft marked by Guardian GI");
        for(auto id:{"E2","NALASR"}) {
            auto u=place(id,foe);sampleDamage(u,ggi);check(!ownsPassive(u,exposed),"infantry and structures excluded from armor piercing");
        }
        check(std::abs(ground->FirepowerMultiplier-.8)<1e-5 && std::abs(rangeState.at(ground->UniqueID)-.75)<1e-5,"smoke weakens firepower and range");
        auto chargeTank=place("YTNK",owner);
        chargeTankId=chargeTank->UniqueID;
        for(int i=0;i<4;++i) chargeTank->GattlingRateUp(1024);
        check(chargeTank->CurrentGattlingStage==chargeTank->GetTechnoType()->WeaponStages-1,
            "native charge reaches the maximum stage without firing");
        markedId=ground->UniqueID;ggiId=ggi->UniqueID;flakId=flak->UniqueID;
        std::ofstream("weapon-marks.ids")<<markedId<<' '<<ggiId<<' '<<flakId<<' '<<chargeTankId;
    }
    if(!loading && frame==91) {
        auto tank=find(chargeTankId);
        check(std::abs(tank->FirepowerMultiplier-1.8)<1e-5,"next state tick grants maximum stage firepower without a shot");
        for(int i=0;i<4;++i) tank->GattlingRateDown(100);
        check(tank->CurrentGattlingStage==0,"native cooldown returns to stage one");
    }
    if(!loading && frame==92)
        check(std::abs(find(chargeTankId)->FirepowerMultiplier-1.0)<1e-5,"next state tick removes firepower after cooling without a shot");
    if(!loading && frame==93) {
        auto ggi=find(ggiId);
        auto reference=place("MTNK",foe,{61,85});
        const int base=sampleDamage(reference,ggi);
        const int boosted=static_cast<int>(base*1.30);
        check(boosted>base+1,"armor piercing lethal test has an overkill margin");
        for(const int hp:{1,base,boosted-1,boosted}) {
            auto target=place("MTNK",foe);
            sampleDamage(target,ggi);
            check(ownsPassive(target,exposed),"lethal test target is marked");
            UnitHealth::heal(target,hp);
            armorPiercingCasualties.push_back(target->UniqueID);
            int damage=20;
            const auto result=target->ReceiveDamage(&damage,0,WarheadTypeClass::Find("AP"),ggi,false,false,owner);
            auto remaining=find(armorPiercingCasualties.back());
            std::ofstream("hex-tactics.log",std::ios::app)<<"lethal piercing: hp "<<hp<<" damage "<<damage
                <<" result "<<static_cast<int>(result)<<" health "<<(remaining?remaining->Health:0)<<'\n';
            check(result==DamageState::NowDead && damage==hp,"armor piercing registers lethal damage");
            settle();
        }
        reference->UnInit();
    }
    if(!loading && frame==99) {
        for(auto id:armorPiercingCasualties) {
            auto tank=find(id);
            check(!tank || !tank->IsAlive,"armor piercing leaves no surviving one-health tank");
        }
    }
    if(!loading && frame==101) {
        auto tank=find(chargeTankId);
        for(int i=0;i<4;++i) tank->GattlingRateUp(1024);
        tickStates();
    }
    if(loading && frame==103) {
        std::ifstream("weapon-marks.ids")>>markedId>>ggiId>>flakId>>chargeTankId;
        auto tank=find(chargeTankId);
        check(tank && tank->CurrentGattlingStage==tank->GetTechnoType()->WeaponStages-1 &&
            std::abs(tank->FirepowerMultiplier-1.8)<1e-5,"loaded full stage retains exactly one firepower bonus");
    }
    if(!loading && frame==100) {
        auto u=find(markedId);sampleDamage(u,find(ggiId));sampleDamage(u,find(flakId));
        check(std::abs(u->FirepowerMultiplier-.8)<1e-5 && adjustPendingDamage("onbeforetakedamage",u,nullptr,nullptr,u,100)==130,
            "successive marks refresh without stacking");
    }
    if(frame==129 || frame==130) {
        if(loading) std::ifstream("weapon-marks.ids")>>markedId>>ggiId>>flakId;
        auto u=find(markedId);check(alive(u),"marked target survives");
        settle();
        const bool active=frame==129;
        check(ownsPassive(u,exposed)==active && std::abs(u->FirepowerMultiplier-(active?.8:1.))<1e-5 &&
              std::abs(unitFactor(markedId,rangeBase,rangeState)-(active?.75:1.))<1e-5,
              "refreshed debuffs expire at thirty frames in fresh and loaded games");
    }
}
}
