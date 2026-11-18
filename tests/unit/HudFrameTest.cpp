#include "HudFrame.h"
#include "starfall_hud_v1.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {
struct Event {
    int x, y, w, h;
    std::vector<uint32_t> colors;
};
struct Fixture {
    int mode = 2;
    bool accept = true;
    int frames = 0, complete = -1;
    std::vector<Event> events;
};
int STARFALL_HUD_CALL begin(void* p, int, int) {
    auto& f = *static_cast<Fixture*>(p);
    f.events.clear();
    return f.mode;
}
int STARFALL_HUD_CALL fill(void* p, int x, int y, int w, int h, uint32_t c) {
    auto& f = *static_cast<Fixture*>(p);
    f.events.push_back({x, y, w, h, {c}});
    return f.accept;
}
int STARFALL_HUD_CALL pixels(void* p, int x, int y, int w, int h, int pitch, const uint8_t* src) {
    assert(pitch >= w * 4);
    auto& f = *static_cast<Fixture*>(p);
    Event event{x, y, w, h, std::vector<uint32_t>(size_t(w) * h)};
    for (int j = 0; j < h; ++j) std::memcpy(event.colors.data() + size_t(j) * w, src + j * pitch, w * 4);
    f.events.push_back(event);
    return f.accept;
}
void STARFALL_HUD_CALL end(void* p, int complete) {
    auto& f = *static_cast<Fixture*>(p);
    ++f.frames;
    f.complete = complete;
}
}

int main() {
    Fixture f;
    StarfallHudSinkV1 sink{sizeof(sink), 1, &f, begin, fill, pixels, end};
    assert(Starfall_SetHudSinkV1(&sink));
    {
        HudFrame::Frame frame(1920, 1080);
        assert(HudFrame::active() && !HudFrame::drawNative());
        assert(!Starfall_SetHudSinkV1(nullptr));
        HudFrame::overPixel(100, 100, 255, 0, 0, 128);
        HudFrame::overPixel(100, 100, 0, 0, 255, 128);
        HudFrame::fill(-8, -8, 2000, 2000, 2, 4, 6, 128);
        assert(HudFrame::beginCard(10, 10, 2, 1));
        HudFrame::fill(10, 10, 1, 1, 1, 2, 3);
        HudFrame::fill(11, 10, 1, 1, 4, 5, 6);
        HudFrame::endCard(60, 60, 4, 2, 128, 61, 61, 3, 1);
    }
    assert(f.frames == 1 && f.complete && f.events.size() == 3);
    const auto& a = f.events[0];
    assert(a.x == 100 && a.y == 100 && a.w == 1 && a.h == 1 && a.colors[0] == 0xc0aa0055);
    const auto& b = f.events[1];
    assert(b.x == 0 && b.y == 0 && b.w == 1920 && b.h == 1080 && b.colors[0] == 0x80060402);
    const auto& c = f.events[2];
    assert(c.x == 61 && c.y == 61 && c.w == 3 && c.h == 1);
    assert((c.colors == std::vector<uint32_t>{0x80030201, 0x80060504, 0x80060504}));
    f.mode = 1;
    {
        HudFrame::Frame frame(8, 8);
        assert(HudFrame::active() && HudFrame::drawNative());
        HudFrame::line(2, 4, 0, 2, 8, 8, 8);
    }
    assert(f.events.size() == 1 && f.events[0].colors[0] && f.events[0].colors[4] &&
           f.events[0].colors[8] && !f.events[0].colors[1]);
    f.accept = false;
    {
        HudFrame::Frame frame(8, 8);
        HudFrame::fill(0, 0, 1, 1, 1, 1, 1);
        assert(!HudFrame::active() && HudFrame::drawNative());
    }
    assert(f.complete == 0);
    f.accept = true;
    { HudFrame::Frame frame(8, 8); }
    assert(f.complete && f.events.empty());
    f.mode = 0;
    { HudFrame::Frame frame(8, 8); assert(!HudFrame::active() && HudFrame::drawNative()); }
    assert(f.frames == 4);
    assert(Starfall_SetHudSinkV1(nullptr));
    sink.version = 42;
    assert(!Starfall_SetHudSinkV1(&sink));
    std::puts("PASS Starfall HUD source-over, alpha cards, clipping, frame order, fallback and clear");
}
