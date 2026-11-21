#define STARFALL_API_BUILD
#include "StarfallAPI.h"
#include "Research.h"
#include <HouseClass.h>
#include <cstring>

uint32_t __cdecl Starfall_GetOwnedResearch(int32_t houseIndex, char* buffer, uint32_t bufferBytes) {
    if(!HouseClass::Array.GetItemOrDefault(houseIndex)) return 0;

    const auto owner=Research::unlocked.find(houseIndex);
    if(owner==Research::unlocked.end() || owner->second.empty()) {
        if(buffer && bufferBytes>=2) std::memset(buffer,0,2);
        return 2;
    }

    const auto& ids=owner->second;
    uint64_t bytes=1;
    for(const auto& id:ids) bytes+=uint64_t(id.size())+1;
    if(bytes>UINT32_MAX) return 0;
    if(buffer && bufferBytes>=bytes) {
        char* out=buffer;
        for(const auto& id:ids) {
            const auto length=id.size()+1;
            std::memcpy(out,id.c_str(),length);
            out+=length;
        }
        *out='\0';
    }
    return static_cast<uint32_t>(bytes);
}
