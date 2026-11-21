#include "../../../src/StarfallAPI.h"
#include <windows.h>
#include <string.h>

/* A separate C DLL, built without Starfall's import library or game headers. */
__declspec(dllexport) int __cdecl CheckOwnedResearch(int32_t house, const char* expected, uint32_t bytes) {
    HMODULE module=GetModuleHandleW(L"Starfall.dll");
    Starfall_GetOwnedResearchFn query=module
        ? (Starfall_GetOwnedResearchFn)GetProcAddress(module,"Starfall_GetOwnedResearch") : NULL;
    char buffer[128], untouched[128];
    if(!query || bytes>=sizeof(buffer) || query(house,NULL,0)!=bytes) return 0;

    memset(untouched,0x55,sizeof(untouched));
    memcpy(buffer,untouched,sizeof(buffer));
    if(query(house,buffer,bytes ? bytes-1 : sizeof(buffer))!=bytes ||
        memcmp(buffer,untouched,sizeof(buffer))!=0) return 0;
    if(bytes==0) return 1;

    if(query(house,buffer,bytes)!=bytes || memcmp(buffer,expected,bytes)!=0) return 0;
    return memcmp(buffer+bytes,untouched+bytes,sizeof(buffer)-bytes)==0;
}
