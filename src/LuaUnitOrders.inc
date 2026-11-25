// These calls run in the synchronized Lua simulation, once on every peer.
// Execute the native order locally, as the unit's house, without broadcasting another input event.
TechnoClass* orderSource(lua_State* L) {
    charge(L,1);
    if(frame(L).damage) luaL_error(L,"unit orders unavailable during damage interception");
    auto unit=live(L,handle(L,1));
    if(!unit || !unit->Owner || !ActiveSkills::readyCaster(unit,unit->Owner) || UnitMount::mounted(unit) ||
       unit->Absorbed || unit->IsImmobilized || unit->IsCrashing || unit->CurrentMission==Mission::Selling ||
       unit->CurrentMission==Mission::Construction) return nullptr;
    int house=unit->Owner->ArrayIndex;
    if(house<0 || house>=HouseClass::Array.Count || HouseClass::Array.Items[house]!=unit->Owner) return nullptr;
    return unit;
}
int submitOrder(lua_State* L,EventClass* order) {
    if(order) {
        order->IsExecuted=false;
        // EventClass::Execute (YR 1.001), shared by the game and extension order handlers.
        reinterpret_cast<void(__thiscall*)(EventClass*)>(0x4C6CB0)(order);
        frame(L).changed=true;
    }
    lua_pushboolean(L,order!=nullptr);
    return 1;
}
int moveOrder(lua_State* L) {
    auto unit=orderSource(L);
    auto cell=pointAt(L,2);
    if(!unit || !abstract_cast<FootClass*>(unit) || unit->GetTechnoType()->Speed<=0) return submitOrder(L,nullptr);
    EventClass order(unit->Owner->ArrayIndex,TargetClass(unit),Mission::Move,TargetClass{},TargetClass(cell),TargetClass{});
    return submitOrder(L,&order);
}
int setDroneGuardCell(lua_State* L) {
    auto unit=orderSource(L);
    auto point=pointAt(L,2);
    using Setter=BOOL(__cdecl*)(TechnoClass*,int,int);
    auto module=GetModuleHandleW(L"ActSp.dll");
    auto set=module?reinterpret_cast<Setter>(GetProcAddress(module,"ActSp_SetDroneGuardCell")):nullptr;
    bool accepted=unit && set && set(unit,point.X,point.Y);
    if(accepted) frame(L).changed=true;
    lua_pushboolean(L,accepted);
    return 1;
}
int attackOrder(lua_State* L) {
    auto unit=orderSource(L);
    AbstractClass* target=nullptr;
    if(lua_istable(L,2)) target=MapClass::Instance.GetCellAt(pointAt(L,2));
    else {
        auto other=live(L,handle(L,2));
        if(other && other->IsOnMap && !other->InLimbo && !other->Transporter && other!=unit) target=other;
    }
    if(!unit || !target) return submitOrder(L,nullptr);
    int slot=unit->SelectWeapon(target);
    auto mount=slot>=0?unit->GetWeapon(slot):nullptr;
    if(!mount || !mount->WeaponType) return submitOrder(L,nullptr);
    auto error=unit->GetFireError(target,slot,true);
    if(error==FireError::ILLEGAL || error==FireError::CANT) return submitOrder(L,nullptr);
    EventClass order(unit->Owner->ArrayIndex,TargetClass(unit),Mission::Attack,TargetClass(target),TargetClass{},TargetClass{});
    return submitOrder(L,&order);
}
int deployOrder(lua_State* L) {
    auto unit=orderSource(L);
    if(!unit || unit->WhatAmI()==AbstractType::Aircraft || !unit->CanDeploySlashUnload()) return submitOrder(L,nullptr);
    EventClass order(unit->Owner->ArrayIndex,EventType::Deploy);
    order.Deploy.Whom=TargetClass(unit);
    return submitOrder(L,&order);
}
