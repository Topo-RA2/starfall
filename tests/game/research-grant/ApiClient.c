#include "../../../src/StarfallAPI.h"
#include "../../../src/starfall_research_display_v1.h"
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

__declspec(dllexport) int __cdecl CheckResearchDisplay(int32_t house, const char* firstId, uint32_t count) {
    typedef uint32_t (__cdecl *ListFn)(int32_t,void*,uint32_t);
    typedef uint32_t (__cdecl *DetailsFn)(uint32_t,const char*,uint32_t,void*,uint32_t);
    typedef int32_t (__cdecl *IconFn)(uint32_t,const char*,uint32_t,uint8_t*,uint32_t);
    HMODULE module=GetModuleHandleW(L"Starfall.dll");
    ListFn list=(ListFn)GetProcAddress(module,"Starfall_GetCompletedResearchV1");
    DetailsFn details=(DetailsFn)GetProcAddress(module,"Starfall_GetResearchDetailsV1");
    IconFn icon=(IconFn)GetProcAddress(module,"Starfall_CopyResearchIconV1");
    unsigned char buffer[4096], untouched[4096];
    uint8_t pixels[STARFALL_RESEARCH_ICON_BYTES_V1+1];
    StarfallCompletedResearchV1 listHeader;
    StarfallResearchDetailsV1 detailHeader;
    uint32_t bytes, idBytes, index;
    int alphaFound=0;
    if(!list || !details || !icon) return 0;
    memset(untouched,0x55,sizeof(untouched));
    memcpy(buffer,untouched,sizeof(buffer));
    if(list(-1,buffer,sizeof(buffer))!=0 || memcmp(buffer,untouched,sizeof(buffer))!=0) return 0;

    bytes=list(house,NULL,0);
    if(bytes<sizeof(listHeader) || bytes>=sizeof(buffer) ||
        list(house,buffer,bytes-1)!=bytes || memcmp(buffer,untouched,sizeof(buffer))!=0 ||
        list(house,buffer,bytes)!=bytes || buffer[bytes]!=0x55) return 0;
    memcpy(&listHeader,buffer,sizeof(listHeader));
    if(!listHeader.catalog_revision || listHeader.count!=count) return 0;
    if(!count) return bytes==sizeof(listHeader);
    idBytes=(uint32_t)strlen(firstId);
    if(bytes<=sizeof(listHeader)+idBytes ||
        memcmp(buffer+sizeof(listHeader),firstId,idBytes+1)!=0) return 0;

    memcpy(buffer,untouched,sizeof(buffer));
    if(details(0,firstId,idBytes,buffer,sizeof(buffer))!=0 ||
        memcmp(buffer,untouched,sizeof(buffer))!=0) return 0;
    bytes=details(listHeader.catalog_revision,firstId,idBytes,NULL,0);
    if(bytes<=sizeof(detailHeader) || bytes>=sizeof(buffer) ||
        details(listHeader.catalog_revision,firstId,idBytes,buffer,bytes-1)!=bytes ||
        memcmp(buffer,untouched,sizeof(buffer))!=0 ||
        details(listHeader.catalog_revision,firstId,idBytes,buffer,bytes)!=bytes || buffer[bytes]!=0x55) return 0;
    memcpy(&detailHeader,buffer,sizeof(detailHeader));
    if(detailHeader.rarity!=2 || !detailHeader.name_bytes || !detailHeader.rarity_name_bytes ||
        !detailHeader.effect_bytes || bytes!=sizeof(detailHeader)+detailHeader.name_bytes+
            detailHeader.rarity_name_bytes+detailHeader.effect_bytes ||
        memchr(buffer+sizeof(detailHeader),0,bytes-sizeof(detailHeader))) return 0;

    memset(pixels,0x55,sizeof(pixels));
    if(icon(listHeader.catalog_revision,firstId,idBytes,pixels,STARFALL_RESEARCH_ICON_BYTES_V1-1) ||
        icon(0,firstId,idBytes,pixels,STARFALL_RESEARCH_ICON_BYTES_V1)) return 0;
    for(index=0;index<sizeof(pixels);++index) if(pixels[index]!=0x55) return 0;
    if(!icon(listHeader.catalog_revision,firstId,idBytes,pixels,STARFALL_RESEARCH_ICON_BYTES_V1) ||
        pixels[STARFALL_RESEARCH_ICON_BYTES_V1]!=0x55) return 0;
    for(index=3;index<STARFALL_RESEARCH_ICON_BYTES_V1;index+=4)
        if(pixels[index]!=0 && pixels[index]!=0x55) alphaFound=1;
    return alphaFound;
}
