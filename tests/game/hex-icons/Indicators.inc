namespace {
void testIndicators() {
    namespace I=ActiveHUD::Indicator;
    auto ring=I::texture("","sfindicator_ring.png",111);
    auto target=I::texture("","sfindicator_target.png",112);
    auto arrow=I::texture("","sfindicator_arrow.png",113);
    check(ring->content.Width>0 && target->content.Width>0 && arrow->content.Width>0,"indicator PNG resources decoded");
    check(I::texture("missing-indicator.png","sfindicator_ring.png",111)==ring,"indicator missing PNG falls back");
    auto view=DSurface::ViewBounds;
    auto surface=DSurface::Composite;
    const int bg=color(18,30,37);
    RectangleStruct full{0,0,surface->GetWidth(),surface->GetHeight()};
    surface->FillRect(&full,bg);
    // Arbitrary nonzero viewport origin, with pixels outside it left untouched.
    DSurface::ViewBounds={32,32,400,260};
    check(I::paint(*ring,I::content(*ring),{-100,-70},{500,40},{-60,400},true,90),"clipped transformed PNG");
    auto pixels=snapshot(full);size_t changed=0;
    for(int y=0;y<full.Height;++y) for(int x=0;x<full.Width;++x) {
        bool inside=x>=32 && x<432 && y>=32 && y<292;
        WORD value=pixels[y*full.Width+x];
        if(!inside) check(value==bg,"indicator never paints outside tactical viewport");
        else changed+=value!=bg;
    }
    check(changed>50,"clipped indicator remains visible");
    DSurface::ViewBounds=view;
    auto oldCamera=TacticalClass::Instance->TacticalPos;
    ActiveSkills::Spec spec;
    CoordStruct origin{16000,16000,0};
    auto anchor=[&](Point2D at) {
        auto world=TacticalClass::CoordsToScreen(origin);
        TacticalClass::Instance->TacticalPos={world.X-at.X+view.X,world.Y-at.Y+view.Y};
    };
    auto horizontal=[&](int distance) {return CoordStruct{origin.X+distance,origin.Y-distance,0};};
    auto src=I::content(*arrow);
    auto shortParts=I::slices(800,spec.arrowWidth*256,src,spec.arrowHead,spec.arrowTail);
    auto large=I::slices(2600,spec.arrowWidth*256,src,spec.arrowHead,spec.arrowTail);
    check(std::abs(shortParts.head-large.head)<0.00001 && std::abs(shortParts.tail-large.tail)<0.00001 &&
        std::abs((large.middle-shortParts.middle)-1800)<0.00001,"only arrow middle changes length");
    auto tiny=I::slices(50,spec.arrowWidth*256,src,spec.arrowHead,spec.arrowTail);
    check(tiny.middle==0 && tiny.tail==0 && tiny.headPixels<src.h*spec.arrowHead/100.0,"short arrow crops instead of scaling caps");
    // Pixel comparison of the same head and tail rendered at two actual lengths.
    double horizontalScale=I::groundVector(1/std::sqrt(2.0),-1/std::sqrt(2.0)).x;
    int headWidth=static_cast<int>(shortParts.head*horizontalScale)-2;
    int tailWidth=static_cast<int>(shortParts.tail*horizontalScale)-2;
    check(headWidth>4 && tailWidth>4,"cap samples exclude stretchable middle");
    auto render=[&](int distance) {
        surface->FillRect(&view,bg);anchor({view.X+80,view.Y+150});
        check(I::direction(origin,horizontal(distance),spec,true),"arrow rasterized");
        auto endpoint=I::screen(horizontal(distance));
        capture(view,distance==768?"arrow-short.ppm":"arrow-long.ppm");
        return std::pair{snapshot({static_cast<int>(endpoint.x)-headWidth,view.Y+125,headWidth,50}),
                         snapshot({view.X+80,view.Y+125,tailWidth,50})};
    };
    auto first=render(768),second=render(1536);
    check(first.first==second.first && first.second==second.second,"arrow cap pixels preserved across lengths");
    check(std::any_of(first.first.begin(),first.first.end(),[&](WORD p){return p!=bg;}),"head sample contains artwork");
    surface->FillRect(&view,bg);anchor({view.X+180,view.Y+100});
    check(I::marker(origin,512,"",true,true,90,0),"projected range PNG");
    RectangleStruct sample{view.X+70,view.Y+45,220,110};
    auto valid=snapshot(sample);
    surface->FillRect(&view,bg);
    check(I::marker(origin,512,"",true,false,90,0),"invalid range PNG");
    auto invalid=snapshot(sample);
    check(valid!=invalid && valid[55*220+110]==bg,"validity tint and transparent ring center");
    auto channel=[](WORD p,int shift,int mask){return ((p>>shift)&mask)*255/mask;};
    int blueCount=0,redCount=0;
    for(size_t i=0;i<valid.size();++i) {
        auto r=channel(valid[i],Drawing::RedShiftLeft,255>>Drawing::RedShiftRight);
        auto g=channel(valid[i],Drawing::GreenShiftLeft,255>>Drawing::GreenShiftRight);
        auto b=channel(valid[i],Drawing::BlueShiftLeft,255>>Drawing::BlueShiftRight);
        blueCount+=b>100 && b>g+20 && g>r+20;
        r=channel(invalid[i],Drawing::RedShiftLeft,255>>Drawing::RedShiftRight);
        b=channel(invalid[i],Drawing::BlueShiftLeft,255>>Drawing::BlueShiftRight);
        redCount+=r>80 && r>b+30;
    }
    check(blueCount>200 && redCount>200,"valid bright blue and invalid red pixels");
    check(I::rotation(0)==I::rotation(I::rotationPeriod) && I::reticleScale(0)==I::reticleScale(I::reticlePeriod),
          "indicator animation loops seamlessly");
    for(ULONGLONG t=0;t<6000;++t) check(I::reticleScale(t)>=0.9 && I::reticleScale(t)<=1.12,"bounded elastic scale");
    auto markerFrame=[&](bool area,ULONGLONG time) {
        surface->FillRect(&view,bg);anchor({view.X+180,view.Y+100});
        check(I::marker(origin,512,"",area,true,90,time),"animated marker rendered");
        return snapshot(sample);
    };
    auto rotated=markerFrame(true,750);
    check(rotated!=valid && markerFrame(true,6000)==valid,"range rotates and returns to original frame");
    auto bounds=[&](const std::vector<WORD>& frame) {
        RectangleStruct r{sample.Width,sample.Height,0,0};int right=0,bottom=0;
        for(int y=0;y<sample.Height;++y) for(int x=0;x<sample.Width;++x) {
            auto p=frame[y*sample.Width+x];
            if(channel(p,Drawing::BlueShiftLeft,255>>Drawing::BlueShiftRight)<100) continue;
            r.X=std::min(r.X,x);r.Y=std::min(r.Y,y);right=std::max(right,x+1);bottom=std::max(bottom,y+1);
        }
        r.Width=right-r.X;r.Height=bottom-r.Y;return r;
    };
    auto before=bounds(valid),after=bounds(rotated);
    check(std::abs(before.Width-after.Width)<=5 && std::abs(before.Height-after.Height)<=5 &&
          std::abs(2*before.X+before.Width-2*after.X-after.Width)<=4 &&
          std::abs(2*before.Y+before.Height-2*after.Y-after.Height)<=4,"rotating range keeps ground footprint and center");
    auto compressed=markerFrame(false,210),expanded=markerFrame(false,480);
    auto tight=bounds(compressed),wide=bounds(expanded);
    check(wide.Width>tight.Width+10 && wide.Height>tight.Height+5 &&
          std::abs(2*tight.X+tight.Width-2*wide.X-wide.Width)<=4 &&
          std::abs(2*tight.Y+tight.Height-2*wide.Y-wide.Height)<=4,"elastic reticle scales about fixed center");
    surface->FillRect(&view,bg);
    I::marker(origin,512,"",true,true,0);
    auto hidden=snapshot(sample);
    check(std::all_of(hidden.begin(),hidden.end(),[&](WORD p){return p==bg;}),"zero opacity leaves pixels untouched");
    // A reproducible contact sheet from the real game surface and renderer.
    surface->FillRect(&view,bg);
    auto label=[&](const wchar_t* text,int x,int y){ResearchText::draw(text,view.X+x,view.Y+y,color(223,235,240),16);};
    label(L"地面范围 / 目标 / 不可施放",20,12);
    anchor({view.X+120,view.Y+100});I::marker(origin,448,"",true,true,90);
    anchor({view.X+280,view.Y+100});I::marker(origin,280,"",false,true,90);
    anchor({view.X+440,view.Y+100});I::marker(origin,448,"",true,false,90);
    label(L"同一箭头：两端不变，仅中段伸长",20,170);
    anchor({view.X+40,view.Y+225});I::direction(origin,horizontal(768),spec,true);
    anchor({view.X+40,view.Y+285});I::direction(origin,horizontal(1536),spec,true);
    label(L"世界空间转向后，按地图投影",20,335);
    anchor({view.X+90,view.Y+395});I::direction(origin,{origin.X+1024,origin.Y,0},spec,true);
    anchor({view.X+350,view.Y+440});I::direction(origin,{origin.X,origin.Y-1024,0},spec,true);
    capture({view.X,view.Y,std::min(view.Width,600),std::min(view.Height,510)},"active-indicator-sheet.ppm");
    // Fixed local timestamps give a reproducible six-second animated preview.
    CreateDirectoryA("indicator-frames",nullptr);
    for(int frame=0;frame<90;++frame) {
        auto time=static_cast<ULONGLONG>(frame)*I::rotationPeriod/90;
        surface->FillRect(&view,bg);
        label(L"范围圈自转",70,20);label(L"目标准星弹性缩放",310,20);
        anchor({view.X+160,view.Y+145});I::marker(origin,640,"",true,true,90,time);
        anchor({view.X+415,view.Y+145});I::marker(origin,400,"",false,true,90,time);
        label(L"有效：亮蓝色     不可施放：红色",110,240);
        char file[100];sprintf_s(file,"indicator-frames/frame-%03d.ppm",frame);
        capture({view.X,view.Y,580,280},file);
    }
    TacticalClass::Instance->TacticalPos=oldCamera;
    Research::clearPresentationCaches();
    check(imageCache().size()==0,"indicator textures released with presentation cache");
    std::ofstream("hex-icons.log",std::ios::app)<<"PASS projected PNG indicators and viewport clipping\nPASS arrow caps preserved across lengths\n"
        <<"PASS animated markers and bright blue validity color\n";
}
}
