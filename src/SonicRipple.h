#pragma once
#include <GeneralDefinitions.h>

class Surface;
namespace SonicRipple {
    void launch(const CoordStruct& from,const CoordStruct& to,unsigned frame);
    void expire(unsigned frame);
    void clear();
    void draw(Surface* surface,RectangleStruct viewport,unsigned frame);
}
