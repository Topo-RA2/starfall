#pragma once
#include <TechnoClass.h>

namespace GuardPoints {
void clearProfiles();
bool eligible(TechnoClass* owner);
CellStruct station(DWORD ownerId,CellStruct fallback);
TechnoClass* findTarget(TechnoClass* guard,CellClass* point,int radius);
void reset();
}
