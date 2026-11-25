#include "SonicRipple.h"
#include <Drawing.h>
#include <MapClass.h>
#include <Surface.h>
#include <TacticalClass.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace SonicRipple {
namespace {
// World-space samples only. No target pointers, game objects, or simulation RNG.
struct Pulse { CoordStruct from,to; unsigned start; };
std::vector<Pulse> pulses;
constexpr unsigned travel=12, separation=3, fronts=3;
constexpr unsigned lifetime=travel+separation*(fronts-1)+1;

void refract(BYTE* pixels,int pitch,int width,int height,int cx,int cy,double ux,double uy,int radius) {
    const WORD tint=static_cast<WORD>(Drawing::RGB_To_Int(174,217,237));
    const WORD lowBits=static_cast<WORD>(1u<<Drawing::RedShiftLeft | 1u<<Drawing::GreenShiftLeft | 1u<<Drawing::BlueShiftLeft);
    for(int y=std::max(0,cy-radius);y<std::min(height,cy+radius+1);++y) {
        auto row=reinterpret_cast<WORD*>(pixels+y*pitch);
        for(int x=std::max(0,cx-radius);x<std::min(width,cx+radius+1);++x) {
            const int dx=x-cx,dy=y-cy;
            const double across=dx*uy-dy*ux;
            if(std::abs(across)>radius) continue;
            // A curved wavefront perpendicular to the direction of travel.
            const double band=dx*ux+dy*uy+0.55*across*across/radius;
            if(band < -1.5 || band > 1.5) continue;
            const int sx=std::clamp(x+static_cast<int>(std::round(2*ux)),0,width-1);
            const int sy=std::clamp(y+static_cast<int>(std::round(2*uy)),0,height-1);
            const WORD sample=reinterpret_cast<WORD*>(pixels+sy*pitch)[sx];
            row[x]=band<0 ? sample : static_cast<WORD>((sample&tint)+(((sample^tint)&~lowBits)>>1));
        }
    }
}
}

void launch(const CoordStruct& from,const CoordStruct& to,unsigned frame) {
    if(from!=to) pulses.push_back({from,to,frame});
}
void clear() { pulses.clear(); }
void expire(unsigned frame) {
    std::erase_if(pulses,[frame](const Pulse& pulse){return frame-pulse.start>=lifetime;});
}
void draw(Surface* surface,RectangleStruct viewport,unsigned frame) {
    if(pulses.empty() || !surface || !TacticalClass::Instance || surface->GetBytesPerPixel()!=2) return;
    const int left=std::max(0,viewport.X),top=std::max(0,viewport.Y);
    const int width=std::min(surface->GetWidth(),viewport.X+viewport.Width)-left;
    const int height=std::min(surface->GetHeight(),viewport.Y+viewport.Height)-top;
    if(width<=0 || height<=0) return;
    BYTE* pixels=nullptr;
    for(const auto& pulse:pulses) {
        const unsigned age=frame-pulse.start;
        if(age>=lifetime) continue;
        const auto a=TacticalClass::Instance->CoordsToClient(pulse.from).first;
        const auto b=TacticalClass::Instance->CoordsToClient(pulse.to).first;
        const int dx=b.X-a.X,dy=b.Y-a.Y;
        const double length=std::hypot(dx,dy);
        if(length<1) continue;
        for(unsigned i=0;i<fronts;++i) {
            const unsigned elapsed=age-i*separation;
            if(elapsed>travel) continue;
            const double t=static_cast<double>(elapsed)/travel;
            const int x=a.X+static_cast<int>(dx*t)+viewport.X-left,y=a.Y+static_cast<int>(dy*t)+viewport.Y-top;
            const int radius=4+static_cast<int>(8*t);
            if(x+radius<0 || y+radius<0 || x-radius>=width || y-radius>=height) continue;
            const auto world=pulse.from+(pulse.to-pulse.from)*t;
            auto cell=MapClass::Instance.TryGetCellAt(world);
            if(!cell || cell->IsShrouded() || cell->IsFogged()) continue;
            if(!pixels) {
                pixels=static_cast<BYTE*>(surface->Lock(left,top));
                if(!pixels) return;
            }
            refract(pixels,surface->GetPitch(),width,height,x,y,dx/length,dy/length,radius);
            TacticalClass::Instance->RegisterDirtyArea({x+left-viewport.X-radius,y+top-viewport.Y-radius,2*radius+1,2*radius+1},false);
        }
    }
    if(pixels) surface->Unlock();
}
}
