#pragma once
#include <stdint.h>

#if defined(_MSC_VER)
#define STARFALL_HUD_CALL __cdecl
#elif defined(__i386__)
#define STARFALL_HUD_CALL __attribute__((cdecl))
#else
#define STARFALL_HUD_CALL
#endif

// Synchronous game-thread callbacks. Coordinates use Composite pixels.
// Images contain straight RGBA8, in source-over order, and are borrowed only
// during the call. A nonzero begin must receive one end, even for an empty HUD.
typedef struct StarfallHudSinkV1 {
    uint32_t size;
    uint32_t version;
    void* context;
    int (STARFALL_HUD_CALL *begin)(void*, int width, int height); // 0 native, 1 dual, 2 GPU
    int (STARFALL_HUD_CALL *fill)(void*, int x, int y, int width, int height, uint32_t rgba);
    int (STARFALL_HUD_CALL *pixels)(void*, int x, int y, int width, int height, int pitch, const uint8_t* rgba);
    void (STARFALL_HUD_CALL *end)(void*, int complete); // zero discards the incomplete frame
} StarfallHudSinkV1;

// Register or clear the sink (NULL) outside a frame on the game thread.
// The DLL copies the table. Nonzero indicates success. fill packs red in bits
// 0..7 and alpha in bits 24..31. Both fill and pixels return nonzero on success.
#ifdef __cplusplus
extern "C" {
#endif
int STARFALL_HUD_CALL Starfall_SetHudSinkV1(const StarfallHudSinkV1* sink);
#ifdef __cplusplus
}
#endif
