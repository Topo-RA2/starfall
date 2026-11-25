DWORD waypointTeamID=0;
int waypointDeathFrame=0;
CellStruct waypointGoal{};
std::map<DWORD,CellStruct> waypointStart;
void trackSplitTeam(int frame) {
    TeamClass* found=nullptr;
    for(auto team:TeamClass::Array) if(team->UniqueID==waypointTeamID) found=team;
    if(!waypointDeathFrame) {
        if(found && found->FirstUnit && found->FirstUnit->Destination && found->CurrentScript->CurrentMission==1) {
            waypointGoal=CellClass::Coord2Cell(found->FirstUnit->Destination->GetCoords());
            int lethal=1000000;
            found->FirstUnit->ReceiveDamage(&lethal,0,RulesClass::Instance->C4Warhead,nullptr,true,false,nullptr);
            waypointDeathFrame=frame;
        } else if(frame>=135) check(false,"original member begins moving to waypoint");
        return;
    }
    if(!found || found->TotalObjects!=2 || waypointStart.empty())
        check(found && found->TotalObjects==2,"last member's split team survives a normal game tick");
    auto distance=[](CellStruct cell){return std::abs(cell.X-waypointGoal.X)+std::abs(cell.Y-waypointGoal.Y);};
    int moved=0;
    for(auto member=found->FirstUnit;member;member=member->NextTeamMember) {
        auto cell=member->GetMapCoords();
        auto [it,added]=waypointStart.emplace(member->UniqueID,cell);
        if(!added && distance(cell)<distance(it->second)) ++moved;
    }
    if(frame==waypointDeathFrame+75) {
        check(waypointStart.size()==2 && moved==2 && !found->AchievedGreatSuccess &&
            found->CurrentScript->CurrentMission>=1,"both split members move without replaying the first script line");
        TerminateProcess(GetCurrentProcess(),0);
    }
}
void enemyResearch(HouseClass* foe) {
    const auto house=foe->ArrayIndex;
    const auto country=foe->Type->ID;
    using Research::Rarity;
    check(Research::grantRandom(house,country,Rarity::Insane)<0 &&
        !Research::grantFree(house,country,{},Rarity::Insane),"enemy projects excluded from random and free pools");
    auto grant=[&](int preset,const char* suffix) {
        const string id=string("sf_hx_enemy")+suffix+"tech";
        const auto before=Research::unlockOrder[house].size();
        check(!Research::owns(house,id) &&
            !Research::grantFree(house,country,{id},std::nullopt),"scenario project excluded from free choice by ID");
        TActionClass action;action.ActionKind=static_cast<TriggerAction>(9602);action.Param3=preset;action.Param4=0;
        CellStruct cell{-1,-1};
        action.Execute(foe,nullptr,nullptr,cell);action.Execute(foe,nullptr,nullptr,cell);
        check(Research::owns(house,id) && Research::unlockOrder[house].size()==before+1 &&
            !Research::owns(HouseClass::CurrentPlayer->ArrayIndex,id),"direct scenario grant succeeds exactly once");
    };
    auto tick=[](){refreshAllLinkedPassives();settle();settle();};
    auto kill=[&](TechnoClass* u,bool reload=false) {
        int damage=1000000;u->ReceiveDamage(&damage,0,RulesClass::Instance->C4Warhead,nullptr,true,false,nullptr);
        if(reload) {
            IStream* stream=nullptr;
            check(SUCCEEDED(CreateStreamOnHGlobal(nullptr,TRUE,&stream)) && saveState(stream),"save pending rebirth");
            LARGE_INTEGER zero{};stream->Seek(zero,STREAM_SEEK_SET,nullptr);
            check(loadState(stream),"restore pending rebirth");stream->Release();
        }
        tick();
    };
    auto survivors=[&](const char* type) {
        vector<TechnoClass*> result;
        for(auto u:TechnoClass::Array) if(alive(u) && u->IsOnMap && u->Owner==foe &&
            !_stricmp(u->GetTechnoType()->ID,type)) result.push_back(u);
        return result;
    };
    auto chance=[](const char* id,int expected,int forced) {
        auto it=skills.find(string("sf_hx_enemy")+id);
        check(it!=skills.end() && it->second.chance==expected,"configured enemy chance");
        it->second.chance=forced;
    };
    // Force only the chosen outcomes, after verifying their production percentages.
    chance("rebirthroll",20,100);grant(890,"rebirth");
    auto u=place("YTNK",foe,{49,64});tick();check(has(u,"sf_hx_enemyrebirthtwo"),"rebirth marked");
    auto makeTeam=[&](TechnoClass* member) {
        auto type=TeamTypeClass::Find("SFCloneTeam");
        check(type && type->ScriptType && type->TaskForce,"map team definition");
        auto team=type->CreateTeam(foe);
        check(team && team->CurrentScript,"map team created with script");
        check(team->AddMember(static_cast<FootClass*>(member),false),"original enters map team");
        team->CurrentScript->CurrentMission=1;team->StepCompleted=false;
        team->IsMoving=true;team->IsHasBeen=true;
        return team;
    };
    auto originalTeam=makeTeam(u);
    for(int life=0;life<3;++life) {
        kill(u,life==1);auto next=survivors("YTNK");check(next.size()==(life<2?1u:0u),"rebirth ends after exactly two copies");
        if(!next.empty()) {
            u=next.front();check(u->Health==u->GetTechnoType()->Strength,"rebirth at full health");
            check(static_cast<FootClass*>(u)->Team==originalTeam && originalTeam->CurrentScript->CurrentMission==1,"rebirth joins original team on current script line");
        }
    }
    // Loading restores the real INI probabilities, including our test override.
    chance("rebirthroll",20,0);
    chance("splitdeath",40,100);grant(891,"split");
    u=place("YTNK",foe,{49,64});tick();const int hp=u->GetTechnoType()->Strength;
    originalTeam=makeTeam(u);kill(u);
    auto minis=survivors("YTNK");check(minis.size()==2,"two same-type split children");
    check(originalTeam->CountObjects[0]==2 && originalTeam->TotalObjects==2,"both split children counted in the original slot");
    for(auto mini:minis) check(static_cast<FootClass*>(mini)->Team==originalTeam &&
        originalTeam->CurrentScript->CurrentMission==1,"both split children follow current team script");
    for(auto mini:minis) {
        check(UnitHealth::read(mini).maximum==(hp*30+50)/100 && UnitHealth::read(mini).current==(hp*30+50)/100 &&
            std::abs(mini->FirepowerMultiplier-0.3)<1e-6 && VisualScale::percent(mini->UniqueID)==60,"mini health damage and size");
        kill(mini);
    }
    check(survivors("YTNK").empty(),"split children cannot split recursively");
    chance("splitdeath",100,0);
    chance("giantroll",12,100);grant(892,"giant");u=place("YTNK",foe,{49,64});tick();
    check(UnitHealth::read(u).maximum==hp*3 && VisualScale::percent(u->UniqueID)==200 &&
        std::abs(u->FirepowerMultiplier-2)<1e-6,"giant form triples health and doubles size and damage");
    chance("giantroll",100,0);
    chance("growthroll",20,100);grant(895,"growth");u=place("YTNK",foe,{49,62});tick();
    check(UnitHealth::read(u).maximum==(hp*5+1)/2 && has(u,"sf_hx_enemyeternal"),"eternal health and form");
    UnitHealth::heal(u,10);tickIntervals();
    periodic.at({u->UniqueID,"sf_hx_enemyeternal"}).start=Unsorted::CurrentFrame-15;
    tickIntervals();dispatch();
    check(UnitHealth::read(u).current==10+std::max(1,(UnitHealth::read(u).maximum+50)/100),"eternal heals one percent each second");
    chance("growthroll",100,0);
    grant(893,"shrink");u=place("YTNK",foe,{49,60});tick();
    check(VisualScale::percent(u->UniqueID)==70 && std::abs(static_cast<FootClass*>(u)->SpeedMultiplier-1.25)<1e-6,"enemy shrink stats");
    grant(894,"frenzy");UnitHealth::heal(u,1);tick();
    check(!ownsPassive(u,"sf_hx_enemyfrenzywatch"),"frenzy consumes its single low-health roll");
    chance("rancorroll",15,100);grant(896,"rancor");tick();
    auto source=place("YTNK",foe,{50,61});tick();check(has(source,"sf_hx_enemyrancorform"),"rancor marked");
    kill(source);check(has(u,"sf_hx_enemyrancorbuff") &&
        warheadGrants.at(u->UniqueID).skills.at("sf_hx_enemyrancorbuff").remaining(Unsorted::CurrentFrame)==75,"rancor death grants nearby haste for five seconds");
    chance("splitdeath",0,100);
    auto last=place("YTNK",foe,{51,62});auto marching=makeTeam(last);tick();
    marching->CurrentScript->CurrentMission=0;marching->StepCompleted=true;
    if(marching->UniqueID==static_cast<DWORD>(-1)) marching->Create_ID();
    waypointTeamID=marching->Fetch_ID();
    // Let the engine run normally between the death and the queued Lua callback.
}
