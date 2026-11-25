#pragma once
#include <TechnoClass.h>

namespace GuardPoints {
void clearProfiles();
bool eligible(TechnoClass* owner);
bool set(TechnoClass* owner,CellStruct cell);
CellStruct station(DWORD ownerId,CellStruct fallback);
TechnoClass* findTarget(TechnoClass* guard,CellClass* point,int radius);
}
