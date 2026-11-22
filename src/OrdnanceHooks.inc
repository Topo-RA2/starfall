DEFINE_HOOK(0x438FCA,SF_BombPlanted,7) {plantedBomb(R->ESI<BombClass*>());return 0;}
// This branch is only reached for an armed bomb with an on-map target.
DEFINE_HOOK(0x43875E,SF_BombImpactBegin,6) {beginBomb(R->ESI<BombClass*>());return 0;}
// Both the vanilla animation block and Phobos's replacement converge here.
DEFINE_HOOK(0x438857,SF_BombImpactEnd,5) {finishBomb(R->ESI<BombClass*>());return 0;}
DEFINE_HOOK(0x4389B0,SF_BombDisarmed,6) {forgetBomb(R->ECX<BombClass*>());return 0;}
DEFINE_HOOK(0x4393F7,SF_BombDestroyed,6) {forgetBomb(R->ESI<BombClass*>());return 0;}

// Ares resolves custom missile damage at 663218; apply the live missile's
// modifiers afterwards, before the native animation and damage paths diverge.
DEFINE_HOOK(0x66321D,SF_RocketImpactBegin,5) {
    auto rocket=static_cast<AircraftClass*>(R->ESI<RocketLocomotionClass*>()->LinkedTo);
    R->EDI(ordnanceDamage(R->EDI<int>(),rocket->FirepowerMultiplier));
    auto scale=ordnanceScale(rocket);
    if(scale!=100) explosionBatches[R->EBP()]={AnimClass::Array.Count,scale};
    return 0;
}
DEFINE_HOOK(0x66328C,SF_RocketExplosionEnd,6) {finishExplosions(R->EBP());return 0;}
