#include "../../src/ResearchText.h"
#include "../../src/player_name_policy.h"
#include <cassert>
#include <cstdio>
int main() {
    const auto keep=[](const wchar_t* name) noexcept { return name; };
    const auto hide=[](const wchar_t*) noexcept -> const wchar_t* { return L"Player"; };
    const std::wstring player=L"TestPlayer";
    assert(PlayerNamePolicy::label(player.c_str(),0,false,nullptr)==player);
    assert(PlayerNamePolicy::label(player.c_str(),0,true,keep)==player);
    assert(PlayerNamePolicy::label(player.c_str(),0,true,hide)==L"Player");
    assert(PlayerNamePolicy::label(player.c_str(),0,true,nullptr)==L"Player");
    assert(PlayerNamePolicy::label(L"",1,false,nullptr)==L"玩家 2");
    assert(player==L"TestPlayer");
    std::puts("PASS research labels keep permitted names and suppress names in anonymous or unsupported hosts");
    ResearchText::Cache cache;
    for(int i=0;i<10000;++i) {
        auto label=L"科研点数 "+std::to_wstring(i)+L" / 100";
        assert(!cache.get(label.c_str(),16).alpha.empty());
        assert(cache.entries.size()<=ResearchText::Cache::maxEntries && cache.bytes<=ResearchText::Cache::maxBytes);
        // Keep a frequently drawn label alive while distinct numbers are evicted.
        const auto& hot=cache.get(L"科研",16);
        assert(&hot==&cache.get(L"科研",16));
    }
    assert(cache.entries.contains({16,L"科研"}) && !cache.entries.contains({16,L"科研点数 0 / 100"}));
    std::printf("PASS 10000 distinct labels, bounded LRU cache: %zu entries, %zu bytes\n",cache.entries.size(),cache.bytes);
}
