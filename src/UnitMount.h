#pragma once
#include <TechnoClass.h>
#include <map>
#include <string>
#include <vector>

namespace UnitMount {
struct Link {
    DWORD parentId=0;
    TechnoClass* parent=nullptr;
    FootClass* child=nullptr;
    std::string slot;
    CoordStruct offset{};
    bool turret=false, transportFire=true;
    std::vector<std::string> excluded;
    int trackerIndex=-1;
    DWORD carrierId=0,orderId=0;
    TechnoClass* ordered=nullptr;
};
inline std::map<DWORD,Link> links;
inline bool mounted(const AbstractClass* unit) {
    return unit && !links.empty() && links.contains(unit->UniqueID);
}
}
