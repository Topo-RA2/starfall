#pragma once

// The host dispatches its own declarations separately so Ares and Phobos
// keep their existing position relative to the Starfall game callbacks.
#include "starfall_yrpp_names.h"
#include <Syringe.h>

#pragma section(".sfhks00", read, write)
#undef declhook
#define declhook(hook, funcname, size) \
    namespace SyringeData::Hooks { \
        __declspec(allocate(".sfhks00")) hookdecl _hk__ ## hook ## funcname {hook, size, #funcname}; \
    } \
    _YR_DEFINE_INCLUDE_ANCHOR(_YR_PP_CAT(YrKeepHook_, _YR_PP_CAT(hook, funcname)), &SyringeData::Hooks::_hk__ ## hook ## funcname)
