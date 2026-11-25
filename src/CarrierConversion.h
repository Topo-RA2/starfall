#pragma once

class TechnoClass;
class TechnoTypeClass;

namespace CarrierConversion {
bool supported(const TechnoClass* carrier);
bool reserveSlots(TechnoClass* carrier,const TechnoTypeClass* destination);
void apply(TechnoClass* carrier,const TechnoTypeClass* destination);
}
