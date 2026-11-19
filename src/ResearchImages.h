#pragma once
#include <Surface.h>
#include <Memory.h>
#include <memory>

namespace ResearchImages {
using Bitmap=std::unique_ptr<BSurface,GameDeleter>;
void drawBadge(Surface* surface,int project,RectangleStruct rect);
Bitmap tooltip(int project,int width,int maxHeight);
}
