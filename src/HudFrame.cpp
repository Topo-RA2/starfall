#include "HudFrame.h"
#include "starfall_hud_v1.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <new>
#include <vector>

#if defined(_MSC_VER) && defined(_M_IX86)
#pragma comment(linker, "/export:Starfall_SetHudSinkV1=_Starfall_SetHudSinkV1")
#endif

namespace {
// A full-view dimmer is one fill command. Rasterize individual icons, glyphs
// and lines into local tiles, so HUD uploads never require a battlefield copy.
constexpr int tileSize = 64;
constexpr size_t maxTiles = 1024;
constexpr int maxDimension = 16384;
constexpr size_t maxCardPixels = 1024 * 1024;
using Pixel = uint32_t;
static_assert(sizeof(Pixel) == 4);

Pixel rgba(int r, int g, int b, int a) {
    return Pixel(r) | (Pixel(g) << 8) | (Pixel(b) << 16) | (Pixel(a) << 24);
}

void over(Pixel& dst, Pixel src) {
    const unsigned sa = src >> 24;
    if (!sa) return;
    const unsigned da = dst >> 24;
    if (!da || sa == 255) { dst = src; return; }
    const unsigned oldWeight = da * (255 - sa);
    const unsigned newWeight = sa * 255;
    const unsigned sum = oldWeight + newWeight;
    Pixel out = ((sum + 127) / 255) << 24;
    for (int shift = 0; shift != 24; shift += 8) {
        const unsigned channel = ((src >> shift & 255) * newWeight + (dst >> shift & 255) * oldWeight + sum / 2) / sum;
        out |= channel << shift;
    }
    dst = out;
}

struct Rect { int x, y, w, h; };
Rect intersect(Rect a, Rect b) {
    if (a.w <= 0 || a.h <= 0 || b.w <= 0 || b.h <= 0) return {};
    const int x = std::max(a.x, b.x), y = std::max(a.y, b.y);
    const int64_t right = std::min(int64_t(a.x) + a.w, int64_t(b.x) + b.w);
    const int64_t bottom = std::min(int64_t(a.y) + a.h, int64_t(b.y) + b.h);
    if (right <= x || bottom <= y) return {};
    return {x, y, int(right - x), int(bottom - y)};
}

struct Tile {
    std::array<Pixel, tileSize * tileSize> pixels{};
    int left = tileSize, top = tileSize, right = 0, bottom = 0;
    void paint(int x, int y, Pixel color) {
        left = std::min(left, x); top = std::min(top, y);
        right = std::max(right, x + 1); bottom = std::max(bottom, y + 1);
        over(pixels[y * tileSize + x], color);
    }
};

struct Recorder {
    StarfallHudSinkV1 sink{};
    int mode = 0;
    bool inside = false, failed = false, cardOpen = false, nativePaused = false;
    Rect screen{}, card{};
    int columns = 0;
    std::vector<std::unique_ptr<Tile>> tiles;
    std::vector<size_t> dirty;
    std::vector<Pixel> cardPixels;

    void flush() {
        for (const size_t i : dirty) {
            const auto& tile = *tiles[i];
            if (!failed) {
                const int x = int(i % columns) * tileSize + tile.left;
                const int y = int(i / columns) * tileSize + tile.top;
                const auto* bytes = reinterpret_cast<const uint8_t*>(&tile.pixels[tile.top * tileSize + tile.left]);
                failed = !sink.pixels(sink.context, x, y, tile.right - tile.left, tile.bottom - tile.top, tileSize * 4, bytes);
            }
            tiles[i].reset();
        }
        dirty.clear();
    }

    void paint(int x, int y, Pixel c) {
        if (!mode || failed || !(c >> 24)) return;
        if (cardOpen) {
            const int64_t dx = int64_t(x) - card.x, dy = int64_t(y) - card.y;
            if (dx >= 0 && dx < card.w && dy >= 0 && dy < card.h)
                over(cardPixels[size_t(dy) * card.w + size_t(dx)], c);
            return;
        }
        if (x < 0 || y < 0 || x >= screen.w || y >= screen.h) return;
        const size_t i = size_t(y / tileSize) * columns + x / tileSize;
        if (!tiles[i]) {
            if (dirty.size() == maxTiles) { flush(); if (failed) return; }
            try {
                auto tile = std::make_unique<Tile>();
                dirty.push_back(i);
                tiles[i] = std::move(tile);
            } catch (const std::bad_alloc&) { failed = true; return; }
        }
        tiles[i]->paint(x % tileSize, y % tileSize, c);
    }
};

Recorder state;
}

extern "C" int STARFALL_HUD_CALL Starfall_SetHudSinkV1(const StarfallHudSinkV1* sink) {
    if (state.inside) return 0;
    if (sink && (sink->size < sizeof(StarfallHudSinkV1) || sink->version != 1 ||
                 !sink->begin || !sink->fill || !sink->pixels || !sink->end)) return 0;
    state.sink = sink ? *sink : StarfallHudSinkV1{};
    return 1;
}

namespace HudFrame {
Frame::Frame(int width, int height) {
    if (state.inside || !state.sink.begin) return;
    owner_ = state.inside = true;
    state.mode = state.sink.begin(state.sink.context, width, height);
    if (!state.mode) return;
    state.failed = width <= 0 || height <= 0 || width > maxDimension || height > maxDimension ||
                   (state.mode != 1 && state.mode != 2);
    state.screen = {0, 0, width, height};
    if (state.failed) return;
    state.columns = (width + tileSize - 1) / tileSize;
    try {
        state.tiles.resize(size_t(state.columns) * ((height + tileSize - 1) / tileSize));
    } catch (const std::bad_alloc&) { state.failed = true; }
}

Frame::~Frame() {
    if (!owner_) return;
    if (state.mode) {
        if (state.cardOpen) state.failed = true;
        state.flush();
        state.sink.end(state.sink.context, !state.failed);
    }
    state.cardOpen = false;
    state.mode = 0;
    state.inside = false;
}

bool active() { return state.mode != 0 && !state.failed; }
bool drawNative() { return state.mode != 2 && !state.nativePaused; }
NativePause::NativePause(bool pause) : old_(state.nativePaused) { state.nativePaused |= pause; }
NativePause::~NativePause() { state.nativePaused = old_; }

void overPixel(int x, int y, int r, int g, int b, int a) {
    state.paint(x, y, rgba(r, g, b, a));
}

void fill(int x, int y, int w, int h, int r, int g, int b, int a) {
    if (!active() || a <= 0) return;
    const Rect rect = intersect({x, y, w, h}, state.cardOpen ? state.card : state.screen);
    if (!rect.w) return;
    const Pixel color = rgba(r, g, b, a);
    if (state.cardOpen) {
        const int dx = rect.x - state.card.x, dy = rect.y - state.card.y;
        for (int j = 0; j < rect.h; ++j) {
            Pixel* row = state.cardPixels.data() + size_t(dy + j) * state.card.w + dx;
            if (a == 255) std::fill_n(row, rect.w, color);
            else for (int i = 0; i < rect.w; ++i) over(row[i], color);
        }
    } else {
        // Prior tiles and this command may overlap. Complete them in order.
        state.flush();
        if (!state.failed)
            state.failed = !state.sink.fill(state.sink.context, rect.x, rect.y, rect.w, rect.h, color);
    }
}

void line(int x1, int y1, int x2, int y2, int r, int g, int b) {
    if (!active()) return;
    // The caller clips to ViewBounds. Use wide arithmetic also for coordinates
    // from worlds larger than the visible viewport.
    const int64_t dx = std::abs(int64_t(x2) - x1), dy = -std::abs(int64_t(y2) - y1);
    if (dx > maxDimension || -dy > maxDimension) { state.failed = true; return; }
    const int sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
    int64_t error = dx + dy;
    const Pixel color = rgba(r, g, b, 255);
    for (;;) {
        state.paint(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        const auto twice = 2 * error;
        if (twice >= dy) { error += dy; x1 += sx; }
        if (twice <= dx) { error += dx; y1 += sy; }
    }
}

bool beginCard(int x, int y, int w, int h) {
    if (!active()) return false;
    if (state.cardOpen || w <= 0 || h <= 0 || int64_t(w) * h > maxCardPixels) {
        state.failed = true;
        return false;
    }
    try { state.cardPixels.assign(size_t(w) * h, 0); }
    catch (const std::bad_alloc&) { state.failed = true; return false; }
    state.card = {x, y, w, h};
    state.cardOpen = true;
    return true;
}

void endCard(int x, int y, int w, int h, int opacity, int clipX, int clipY, int clipW, int clipH) {
    if (!state.cardOpen) return;
    state.cardOpen = false;
    if (!active() || opacity <= 0) return;
    const Rect bounds = intersect(intersect({x, y, w, h}, state.screen), {clipX, clipY, clipW, clipH});
    for (int j = 0; j < bounds.h; ++j) {
        const int py = bounds.y + j;
        const size_t row = size_t((int64_t(py) - y) * state.card.h / h) * state.card.w;
        for (int i = 0; i < bounds.w; ++i) {
            const int px = bounds.x + i;
            const Pixel source = state.cardPixels[row + size_t((int64_t(px) - x) * state.card.w / w)];
            const uint32_t a = ((source >> 24) * opacity + 127) / 255;
            state.paint(px, py, (source & 0xffffff) | (a << 24));
        }
    }
}
}
