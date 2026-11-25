namespace GroundCheck {
constexpr CellStruct site{80,84}, rallyPoint{88,84}, luaPoint{88,90};
DWORD childId=0, enemyId=0;
bool shot=false, done=false;

// A second outpost is already on cooldown when the shared frame-120 save is made.
void checkCooldown(int frame,bool load,HouseClass* house) {
    constexpr CellStruct secondSite{75,90}, station{73,90};
    if(frame<20 || frame>440) return;
    if(frame==20 && !load) placeBuilding("ASGNDPOST",secondSite,house)->IronCurtain(600,house,false);
    auto post=outpost(secondSite);
    if(frame==50 && !load) rally(post,station);
    if(frame!=96 && (frame<121 || frame>403) && frame!=440) return;
    auto nearby=[&](HouseClass* owner) -> UnitClass* {
        for(auto u:UnitClass::Array) {
            auto pos=u->GetMapCoords();
            if(u->Owner==owner && u->IsAlive && !u->InLimbo && u->Health>0 && !_stricmp(u->Type->ID,"MTNK") &&
               abs(pos.X-station.X)<=2 && abs(pos.Y-station.Y)<=2) return u;
        }
        return nullptr;
    };
    if(frame==96 && !load) {
        auto u=nearby(house);
        check(u!=nullptr,"second ground slot filled before save");
        int damage=u->Health+1000;
        u->ReceiveDamage(&damage,0,RulesClass::Instance->C4Warhead,nullptr,true,false,nullptr);
    }
    if(frame>=121 && frame<404 && nearby(house)) check(false,"ground cooldown shortened after save");
    if(frame==440) {
        check(post && nearby(house),"pending ground cooldown survived save");
        post->UnInit();
    }
}

UnitClass* findGuard(HouseClass* house) {
    UnitClass* found=nullptr;
    for(auto u:UnitClass::Array) {
        if(u->Owner!=house || u->InLimbo || !u->IsAlive || u->Health<=0 || _stricmp(u->Type->ID,"MTNK")) continue;
        auto at=u->GetMapCoords();
        if(at.X<77 || at.Y<80 || at.X>94 || at.Y>95) continue;
        if(found) check(false,"one ground guard per post");
        found=u;
    }
    return found;
}
void tick(int frame,bool load,HouseClass* house) {
    static bool detour=false, replacement=false;
    static int lost=0;
    if(done || frame<20) return;
    checkCooldown(frame,load,house);
    if(frame==20 && !load) {
        auto b=placeBuilding("ASGNDPOST",site,house);
        check(!b->SpawnManager,"ground post uses a real vehicle");
        for(int x=-1;x<=1;++x) for(int y=-1;y<=1;++y)
            if(x || y) placeBuilding("GAPILL",{short(site.X+x),short(site.Y+y)},house);
        for(short y=83;y<=85;++y) placeBuilding("GAPILL",{84,y},house);
    }
    auto post=outpost(site);
    auto u=findGuard(house);
    if(frame==50 && !load) rally(post,rallyPoint);
    if(frame==80 && !load) {
        check(!u && post->ArchiveTarget==MapClass::Instance.GetCellAt(rallyPoint),"ground guard waits for a free exit");
        for(int x=-1;x<=1;++x) for(int y=-1;y<=1;++y)
            if(x || y) outpost({short(site.X+x),short(site.Y+y)})->UnInit();
    }
    if(u) {
        if(u->GetHeight()!=0) check(false,"ground guard stayed on tracks");
        auto at=u->GetMapCoords();
        if(at.X==84) {
            if(at.Y>=83 && at.Y<=85) check(false,"ground guard crossed a building");
            detour=true;
        }
    }
    if(frame==119 && !load) {
        check(u!=nullptr,"free exit produced one gray tank");
        std::ofstream("ground-identity.txt")<<u->UniqueID;
    }
    if(frame==136) {
        std::ifstream("ground-identity.txt")>>childId;
        check(u && childId && u->UniqueID==childId,"same ground guard after save");
    }
    if(frame==340) {
        check(u && u->UniqueID==childId && u->GetMapCoords()==rallyPoint && (load || detour),
              "ground guard drove around buildings to rally");
        check(grantToUnit(post,PassiveGrant{{"orderdrone"},-1}),"granted Lua ground order");
        tickGains();dispatch();
    }
    if(frame==510) {
        check(u && u->UniqueID==childId && u->GetMapCoords()==luaPoint,"Lua retasked the same ground guard");
        HouseClass* foe=nullptr;
        for(auto h:HouseClass::Array) if(h && !h->IsNeutral() && !h->Defeated && !house->IsAlliedWith(h)) {foe=h;break;}
        check(foe!=nullptr,"ground opponent");
        enemyId=placeBuilding("GAPOWR",{88,92},foe)->UniqueID;
    }
    if(shot && !lost) {
        check(u && u->UniqueID==childId,"ground guard fired its own cannon");
        auto enemy=outpost({88,92});
        int damage=u->Health+1000;
        u->ReceiveDamage(&damage,0,RulesClass::Instance->C4Warhead,enemy,true,false,enemy->Owner);
        enemy->UnInit();
        lost=frame;
    } else if(lost && !replacement) {
        if(u) {
            check(frame-lost>=300 && frame-lost<=320 && u->UniqueID!=childId,"one ground replacement after delay");
            replacement=true;
        } else if(frame-lost>320) check(false,"ground replacement due");
    } else if(replacement && u && u->GetMapCoords()==luaPoint) {
        post->UnInit();
        check(findGuard(house)==u,"surviving guard released on post removal");
        done=true;
    }
}
}
