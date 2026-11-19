#pragma once

#include <Windows.h>
#include <string>

namespace PlayerNamePolicy {
using HostFilter=const wchar_t* (__cdecl *)(const wchar_t*) noexcept;

inline std::wstring label(const wchar_t* name,int playerIndex,bool platformHosted,HostFilter filter) {
    if(platformHosted) {
        if(!filter) return L"Player";
        name=filter(name);
    }
    if(name && *name) return name;
    return L"玩家 "+std::to_wstring(playerIndex+1);
}

inline std::wstring label(const wchar_t* name,int playerIndex) {
    const auto host=GetModuleHandleW(L"MRGA2.dll");
    const auto filter=host?reinterpret_cast<HostFilter>(GetProcAddress(host,"MRGA_VisiblePlayerNameV1")):nullptr;
    return label(name,playerIndex,host!=nullptr,filter);
}
}
