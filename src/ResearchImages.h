#pragma once
#include <Surface.h>
#include <Memory.h>
#include <cstdint>
#include <memory>

namespace ResearchImages {
using Bitmap=std::unique_ptr<BSurface,GameDeleter>;
void drawBadge(Surface* surface,int project,RectangleStruct rect);
// Call on the game thread with a valid project and a full ABI-sized destination.
bool copyProjectIcon(int project,std::uint8_t* rgba);
Bitmap tooltip(int project,int width,int maxHeight);
}
