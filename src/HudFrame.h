#pragma once

namespace HudFrame {
class Frame {
public:
    Frame(int width, int height);
    ~Frame();
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
private:
    bool owner_ = false;
};

class NativePause {
public:
    explicit NativePause(bool pause);
    ~NativePause();
    NativePause(const NativePause&) = delete;
    NativePause& operator=(const NativePause&) = delete;
private:
    bool old_;
};

bool active();
bool drawNative();
void overPixel(int x, int y, int r, int g, int b, int a = 255);
void fill(int x, int y, int w, int h, int r, int g, int b, int a = 255);
void line(int x1, int y1, int x2, int y2, int r, int g, int b);
// Card pixels use the same absolute coordinates as the game. The isolated
// layer is resampled on close, without reading the battlefield background.
bool beginCard(int x, int y, int w, int h);
void endCard(int x, int y, int w, int h, int opacity, int clipX, int clipY, int clipW, int clipH);
}
