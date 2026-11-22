// Ivan bombs and rocket locomotors do not use BulletClass::Detonate.
// Keep per-bomb launch snapshots independent of the planter's lifetime.
struct BombSnapshot {
    double firepower=1;
    int scale=100,owner=-1;
    vector<string> scripts;
};
std::map<DWORD,BombSnapshot> bombSnapshots;
// Native BombClass::UniqueID is UINT32_MAX. One attached bomb is allowed per
// target, so serialize the target's stable ID and rebuild pointer keys on load.
std::map<BombClass*,DWORD> bombKeys;
bool rebindBombKeys=false;
void bindBombKeys() {
    if(!rebindBombKeys) return;
    for(auto bomb:BombListClass::Instance.Bombs)
        if(bomb && bomb->Target && bombSnapshots.contains(bomb->Target->UniqueID))
            bombKeys[bomb]=bomb->Target->UniqueID;
    rebindBombKeys=false;
}
void forgetBomb(BombClass* bomb) {
    bindBombKeys();
    if(auto key=bombKeys.extract(bomb);!key.empty()) bombSnapshots.erase(key.mapped());
}
struct BombImpact {
    BombClass* bomb=nullptr;
    BombSnapshot snapshot;
    bool damaged=false;
    int damage=0;
    CoordStruct point{};
};
vector<BombImpact> bombImpacts;

int ordnanceScale(TechnoClass* source) {
    if(!source) return 100;
    auto fixed=projectileBase.find(source->UniqueID);
    auto state=projectileState.find(source->UniqueID);
    double value=(fixed==projectileBase.end()?100:fixed->second)*(state==projectileState.end()?1.0:state->second);
    return static_cast<int>(std::lround(std::clamp(value,1.0,double(INT_MAX))));
}
int ordnanceDamage(int damage,double factor) {
    return static_cast<int>(std::clamp(std::round(damage*factor),double(INT_MIN),double(INT_MAX)));
}
void plantedBomb(BombClass* bomb) {
    if(!bomb || !bomb->Owner || !bomb->Target) return;
    if(!ready) readSkills();
    BombSnapshot snapshot;
    snapshot.firepower=bomb->Owner->FirepowerMultiplier;
    snapshot.scale=ordnanceScale(bomb->Owner);
    snapshot.owner=bomb->OwnerHouse?bomb->OwnerHouse->ArrayIndex:-1;
    interested(bomb->Owner,"onbombdetonate",&snapshot.scripts);
    if(snapshot.firepower!=1 || snapshot.scale!=100 || !snapshot.scripts.empty()) {
        bindBombKeys();
        auto id=bomb->Target->UniqueID;
        bombKeys[bomb]=id;bombSnapshots[id]=std::move(snapshot);
    }
}
void beginBomb(BombClass* bomb) {
    bindBombKeys();
    BombImpact impact{};impact.bomb=bomb;
    if(auto key=bombKeys.extract(bomb);!key.empty())
        if(auto node=bombSnapshots.extract(key.mapped());!node.empty()) impact.snapshot=std::move(node.mapped());
    bombImpacts.push_back(std::move(impact));
}
void finishBomb(BombClass* bomb) {
    if(bombImpacts.empty() || bombImpacts.back().bomb!=bomb) return;
    auto impact=std::move(bombImpacts.back());bombImpacts.pop_back();
    auto point=CellClass::Coord2Cell(impact.point);
    if(!impact.damaged || impact.snapshot.scripts.empty() || impact.snapshot.owner<0 || impact.snapshot.owner>=HouseClass::Array.Count ||
       !MapClass::Instance.TryGetCellAt(point) || !MapClass::Instance.IsWithinUsableArea(point,false)) return;
    Objects live;for(auto unit:TechnoClass::Array) if(alive(unit)) live.emplace(unit->UniqueID,unit);
    auto previous=activeObjects;activeObjects=&live;
    for(const auto& name:impact.snapshot.scripts) {
        auto found=skills.find(name);
        if(found==skills.end() || found->second.trigger!="onbombdetonate" || ActionSequence::lastCast==UINT32_MAX) continue;
        auto id=++ActionSequence::lastCast;
        auto& cast=ActionSequence::casts.emplace(id,ActionSequence::Cast{"@bomb:"+name,impact.snapshot.owner,point}).first->second;
        auto event=sequenceEvent(id,cast);event.damage=impact.damage;event.origin=event.impact=impact.point;
        runScript(found->second.actions.front(),event,live,0,nullptr);
        ActionSequence::retire(id);
    }
    activeObjects=previous;
}

