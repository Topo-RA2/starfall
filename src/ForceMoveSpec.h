#pragma once
#include <optional>

namespace ForceMove {
enum class Easing { Linear, EaseOut, EaseIn, EaseInOut };
enum class Mode { Offset, Push, Pull, Launch };
struct Spec {
    Mode mode=Mode::Offset;
    Easing easing=Easing::Linear;
    int dx=0,dy=0,distance=0,height=0,duration=0;
    int airborneHeight=256;
    std::optional<double> blowWeight;
    bool impact=false,tilt=true,ignoreCollision=true,affectsIronCurtain=false;
};
}
