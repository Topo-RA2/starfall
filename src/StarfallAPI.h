#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Win32 game thread, after scenario initialization or save restoration.
   houseIndex is HouseClass::ArrayIndex. IDs use the normalized INI spelling,
   sorted by ID. Each ID ends in NUL and one extra NUL terminates the list.
   An empty list contains two NUL bytes.
   Returns the required byte count, or 0 for an invalid house. A NULL or
   undersized buffer is never written. The caller owns the buffer. */
typedef uint32_t (__cdecl *Starfall_GetOwnedResearchFn)(
    int32_t houseIndex, char* buffer, uint32_t bufferBytes);

#ifdef STARFALL_API_BUILD
__declspec(dllexport)
#else
__declspec(dllimport)
#endif
uint32_t __cdecl Starfall_GetOwnedResearch(int32_t houseIndex, char* buffer, uint32_t bufferBytes);

#ifdef __cplusplus
}
#endif
