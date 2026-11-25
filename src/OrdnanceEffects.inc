// Ivan bombs and rocket locomotors do not use BulletClass::Detonate.
// Keep per-bomb launch snapshots independent of the planter's lifetime.
struct BombSnapshot {
    double firepower=1;
    int scale=100,owner=-1;
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
    DWORD areaFrame=0;
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
    if(snapshot.firepower!=1 || snapshot.scale!=100) {
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
    bombImpacts.pop_back();
}
