// A projectile can do direct damage and call DamageArea during the same detonation.
struct WarheadScriptImpact {
    WarheadTypeClass* wh;
    CoordStruct at;
    HouseClass* house;
    int damage;
};
std::map<DWORD,WarheadScriptImpact> warheadScriptImpacts;
std::set<WarheadTypeClass*> runningWarheadScripts;

void finishWarheadScript(DWORD frame) {
    auto pending=warheadScriptImpacts.extract(frame);
    if(pending.empty()) return;
    const auto& hit=pending.mapped();
    auto cell=CellClass::Coord2Cell(hit.at);
    auto house=hit.house?hit.house:HouseClass::FindNeutral();
    if(!house || !MapClass::Instance.TryGetCellAt(cell) || !MapClass::Instance.IsWithinUsableArea(cell,false) ||
       ActionSequence::lastCast==UINT32_MAX || !runningWarheadScripts.insert(hit.wh).second) return;

    auto id=++ActionSequence::lastCast;
    auto& cast=ActionSequence::casts.emplace(id,ActionSequence::Cast{"@warhead:"+string(hit.wh->ID),house->ArrayIndex,cell}).first->second;
    auto event=sequenceEvent(id,cast);event.damage=hit.damage;event.origin=event.impact=hit.at;
    Objects live;
    auto previous=activeObjects;
    if(!previous) {
        for(auto unit:TechnoClass::Array) if(alive(unit)) live.emplace(unit->UniqueID,unit);
        activeObjects=&live;
    }
    runScript(ActionSequence::warheadScripts.at(hit.wh)->actions.front(),event,*activeObjects,0,nullptr);
    activeObjects=previous;
    ActionSequence::retire(id);
    runningWarheadScripts.erase(hit.wh);
}
