#pragma once

class BuildingClass;
class BuildingTypeClass;

namespace BuildingTransform {
bool sameFoundation(const BuildingTypeClass* from,const BuildingTypeClass* to);
bool eligible(BuildingClass* building,BuildingTypeClass* to);
// Called after identity handoff and before retiring the old native object.
using Rebind=void(*)(BuildingClass* previous,BuildingClass* current);
BuildingClass* convert(BuildingClass* building,BuildingTypeClass* to,Rebind rebind);
}
