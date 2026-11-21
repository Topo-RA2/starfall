#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef STARFALL_API_BUILD
#define STARFALL_RESEARCH_DISPLAY_EXPORT __declspec(dllexport)
#else
#define STARFALL_RESEARCH_DISPLAY_EXPORT
#endif

#define STARFALL_RESEARCH_DISPLAY_MAX_BYTES_V1 (1024u * 1024u)
#define STARFALL_RESEARCH_ICON_SIZE_V1 48u
#define STARFALL_RESEARCH_ICON_BYTES_V1 (STARFALL_RESEARCH_ICON_SIZE_V1 * STARFALL_RESEARCH_ICON_SIZE_V1 * 4u)

/*
 * Optional Win32 C ABI. Resolve all three undecorated exports from the loaded
 * Starfall module. Call on the game thread after scenario initialization or
 * save restoration. All storage belongs to the caller. No function retains
 * input pointers or changes simulation state.
 *
 * List and details return the exact required byte count (0 if unavailable,
 * invalid, or beyond MAX_BYTES). A null/short output buffer is never written.
 * Disabled or uninitialized research is unavailable. A valid house with
 * no completed research returns a valid header with count 0.
 * Use HouseClass::ArrayIndex, not the scoreboard row or a displayed name.
 */
/* On Win32 the list header is 8 bytes and the details header is 16 bytes. */
typedef struct StarfallCompletedResearchV1 {
    uint32_t catalog_revision;
    uint32_t count;
    /* Followed by count nonempty NUL-terminated project IDs in acquisition
       order, with legacy entries in configured order. IDs are opaque bytes. */
} StarfallCompletedResearchV1;

typedef struct StarfallResearchDetailsV1 {
    uint32_t rarity; /* 0 common, 1 rare, 2 epic, 3 insane, 4 unavailable */
    uint32_t name_bytes;
    uint32_t rarity_name_bytes;
    uint32_t effect_bytes;
    /* Followed by name, rarity name and effect in UTF-8, without NULs. */
} StarfallResearchDetailsV1;

/*
 * Pass an ID from the completed list and its catalog revision to the detail
 * and icon functions. id_bytes excludes the NUL. Re-read the list after a
 * scene or catalog change before using IDs to request display resources.
 */
STARFALL_RESEARCH_DISPLAY_EXPORT uint32_t __cdecl Starfall_GetCompletedResearchV1(
    int32_t house_index, void* buffer, uint32_t buffer_bytes);
STARFALL_RESEARCH_DISPLAY_EXPORT uint32_t __cdecl Starfall_GetResearchDetailsV1(
    uint32_t catalog_revision, const char* id, uint32_t id_bytes, void* buffer, uint32_t buffer_bytes);

/*
 * Returns 1 on success, or 0 without writing on failure. The icon is
 * 48 x 48 pixels, top-down, tightly packed red/green/blue/alpha bytes with
 * straight alpha. The caller supplies at least ICON_BYTES bytes.
 */
STARFALL_RESEARCH_DISPLAY_EXPORT int32_t __cdecl Starfall_CopyResearchIconV1(
    uint32_t catalog_revision, const char* id, uint32_t id_bytes, uint8_t* rgba, uint32_t rgba_bytes);

#undef STARFALL_RESEARCH_DISPLAY_EXPORT

#ifdef __cplusplus
}
#endif
