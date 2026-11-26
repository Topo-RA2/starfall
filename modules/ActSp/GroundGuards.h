#pragma once
#include <UnitClass.h>
#include <map>

namespace GroundGuards {
// The child uses the engine's saved TargetClass ID, never a process address.
struct Slot {
    int child=0;
    DWORD readyFrame=0;
    int house=-1;
};
extern std::map<DWORD,Slot> slots;
struct Config {
    UnitTypeClass* type=nullptr;
    CellStruct initialPoint{-1,-1};
    int radius=5;
    int delay=300;
};
bool mayHaveConfiguredType();
const Config* config(BuildingClass* building);
}
