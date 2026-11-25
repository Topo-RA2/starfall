#pragma once
#include <TargetClass.h>
#include <map>

namespace BetterGarrison {
struct Order {
    int house;
    int target;
    AbstractType kind;
};

extern std::map<DWORD,Order> orders;
void tick();
void reset();
}
