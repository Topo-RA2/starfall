#pragma once
#include <TargetClass.h>
#include <map>

class EventClass;

namespace BetterGarrison {
struct Order {
    int house;
    int target;
    AbstractType kind;
};

extern std::map<DWORD,Order> orders;
void tick();
void reset();
bool isOccupant(TechnoClass* unit);
void applyOrder(TechnoClass* unit,EventClass* event);
}
