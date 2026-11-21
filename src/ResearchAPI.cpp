#define STARFALL_API_BUILD
#include "StarfallAPI.h"
#include "starfall_research_display_v1.h"
#include "Research.h"
#include "ResearchImages.h"
#include <HouseClass.h>
#include <cstring>
#include <exception>
#include <string_view>

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

namespace {
static_assert(sizeof(StarfallCompletedResearchV1)==8);
static_assert(sizeof(StarfallResearchDetailsV1)==16);

int findDisplayProject(uint32_t revision,const char* id,uint32_t bytes) {
    if(!Research::readyForDisplay() || !revision || revision!=Research::displayCatalogRevision ||
       !id || !bytes || bytes>=STARFALL_RESEARCH_DISPLAY_MAX_BYTES_V1) return -1;
    const std::string_view key(id,bytes);
    for(size_t i=0;i<Research::options.size();++i)
        if(Research::options[i].id==key) return static_cast<int>(i);
    return -1;
}

std::string toUtf8(std::wstring_view text) {
    if(text.empty() || text.size()>STARFALL_RESEARCH_DISPLAY_MAX_BYTES_V1) return {};
    const auto length=static_cast<int>(text.size());
    const int bytes=WideCharToMultiByte(CP_UTF8,0,text.data(),length,nullptr,0,nullptr,nullptr);
    if(bytes<=0 || static_cast<uint32_t>(bytes)>STARFALL_RESEARCH_DISPLAY_MAX_BYTES_V1) return {};
    std::string utf8(static_cast<size_t>(bytes),'\0');
    if(!WideCharToMultiByte(CP_UTF8,0,text.data(),length,utf8.data(),bytes,nullptr,nullptr)) return {};
    return utf8;
}
}

uint32_t __cdecl Starfall_GetCompletedResearchV1(int32_t houseIndex,void* buffer,uint32_t bufferBytes) {
    if(!Research::readyForDisplay() || !Research::displayCatalogRevision) return 0;
    const auto house=HouseClass::Array.GetItemOrDefault(houseIndex);
    if(!house || !house->Type || !house->Type->Multiplay || house->Type->MultiplayPassive ||
       house->IsInitiallyObserver()) return 0;

    try {
        const auto projects=Research::loadedCompletedProjects(houseIndex);
        uint32_t bytes=sizeof(StarfallCompletedResearchV1);
        for(const int index:projects) {
            const auto& id=Research::options[index].id;
            if(id.empty() || id.find('\0')!=std::string::npos ||
               id.size()>=STARFALL_RESEARCH_DISPLAY_MAX_BYTES_V1-bytes) return 0;
            bytes+=static_cast<uint32_t>(id.size()+1);
        }
        if(buffer && bufferBytes>=bytes) {
            const StarfallCompletedResearchV1 header{Research::displayCatalogRevision,static_cast<uint32_t>(projects.size())};
            std::memcpy(buffer,&header,sizeof(header));
            auto out=static_cast<char*>(buffer)+sizeof(header);
            for(const int index:projects) {
                const auto& id=Research::options[index].id;
                const auto length=id.size()+1;
                std::memcpy(out,id.c_str(),length);
                out+=length;
            }
        }
        return bytes;
    } catch(const std::exception&) { return 0; }
}

uint32_t __cdecl Starfall_GetResearchDetailsV1(
    uint32_t revision,const char* id,uint32_t idBytes,void* buffer,uint32_t bufferBytes) {
    const int index=findDisplayProject(revision,id,idBytes);
    if(index<0) return 0;

    try {
        const auto& project=Research::options[index];
        wchar_t conditions[96]{};
        auto effect=Research::describe(index,conditions);
        if(conditions[0]) {
            if(!effect.empty()) effect+=L'\n';
            effect+=conditions;
        }
        const std::string nameUtf8=toUtf8(project.name);
        const std::string rarityUtf8=toUtf8(Research::rarityName(project.rarity));
        const std::string effectUtf8=toUtf8(effect);
        if((!project.name.empty() && nameUtf8.empty()) || (!effect.empty() && effectUtf8.empty())) return 0;

        const size_t bytes=sizeof(StarfallResearchDetailsV1)+nameUtf8.size()+rarityUtf8.size()+effectUtf8.size();
        if(bytes>STARFALL_RESEARCH_DISPLAY_MAX_BYTES_V1) return 0;
        if(buffer && bufferBytes>=bytes) {
            const StarfallResearchDetailsV1 header{
                static_cast<uint32_t>(project.rarity),static_cast<uint32_t>(nameUtf8.size()),
                static_cast<uint32_t>(rarityUtf8.size()),static_cast<uint32_t>(effectUtf8.size())};
            std::memcpy(buffer,&header,sizeof(header));
            auto out=static_cast<char*>(buffer)+sizeof(header);
            for(const auto* field:{&nameUtf8,&rarityUtf8,&effectUtf8}) {
                std::memcpy(out,field->data(),field->size());
                out+=field->size();
            }
        }
        return static_cast<uint32_t>(bytes);
    } catch(const std::exception&) { return 0; }
}

int32_t __cdecl Starfall_CopyResearchIconV1(
    uint32_t revision,const char* id,uint32_t idBytes,uint8_t* rgba,uint32_t rgbaBytes) {
    if(!rgba || rgbaBytes<STARFALL_RESEARCH_ICON_BYTES_V1) return 0;
    const int index=findDisplayProject(revision,id,idBytes);
    if(index<0) return 0;
    try { return ResearchImages::copyProjectIcon(index,rgba)?1:0; }
    catch(const std::exception&) { return 0; }
}
