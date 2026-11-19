namespace {
void testUIAlpha() {
    auto surface=DSurface::Composite;auto view=DSurface::ViewBounds;
    Research::clearPresentationCaches();
    PngImage fixture;fixture.width=fixture.height=16;fixture.pixels.resize(16*16*4);
    for(int y=0;y<16;++y) for(int x=0;x<16;++x) {
        auto p=&fixture.pixels[(y*16+x)*4];p[0]=30;p[1]=70;p[2]=240;p[3]=x<4?0:x<8?128:255;
        if(x>=12 && y>=12) p[0]=p[1]=p[2]=0; // Opaque black is not a transparency key.
    }
    auto cache=[&](const char* path,int resource,const PngImage& image) {
        return imageCache().get(ImageKey{path,resource,Research::Rarity::Common},[&]{return image;},
            [](const auto&,const auto& art){return sizeof(PngImage)+art.pixels.capacity();});
    };
    auto card=cache("sfrcard.png",108,fixture);
    cache("alpha-icon.png",0,fixture);cache("sfactive_default.png",110,fixture);
    auto empty=fixture;std::fill(empty.pixels.begin(),empty.pixels.end(),0);cache("alpha-empty.png",0,empty);
    auto background=[&](int variant=0) {
        auto base=static_cast<BYTE*>(surface->Lock(view.X,view.Y));check(base!=nullptr,"alpha background lock");
        int pitch=surface->GetPitch();
        for(int y=0;y<view.Height;++y) {
            auto row=reinterpret_cast<WORD*>(base+y*pitch);
            for(int x=0;x<view.Width;++x) row[x]=static_cast<WORD>((x/19+y/13+variant)%2?color(77,119,45):color(38,72,26));
        }
        surface->Unlock();
    };
    RectangleStruct r{view.X+32,view.Y+32,96,144};
    for(int variant:{0,1}) {
        background(variant);drawPng(fixture,r,0);auto expected=snapshot(r);
        background(variant);drawCardSkin(r);
        check(snapshot(r)==expected,"cached card preserves zero partial and full alpha");
        check(cardSkinCache().alpha.size()==cardSkinCache().pixels.size(),"card alpha cached with foreground");
    }
    RectangleStruct cropped{-12,-9,96,144},visible{0,0,84,135};
    background();drawPng(fixture,cropped,0);auto expected=snapshot(visible);
    background();drawCardSkin(cropped);
    check(snapshot(visible)==expected,"card alpha clipping matches PNG renderer");
    ActiveSkills::Spec spec;spec.icon="alpha-icon.png";
    RectangleStruct button{view.X+180,view.Y+32,64,64};
    background();blit("sfframe.png",button);auto frame=snapshot(button);
    for(bool enabled:{true,false}) {
        background();ActiveHUD::drawButtonArt(spec,button,enabled);auto actual=snapshot(button);
        int holes=0;
        for(int y=9;y<53;++y) for(int x=10;x<54;++x) if((x-5)*16/54<4) {
            check(actual[y*64+x]==frame[y*64+x],"active icon transparency survives ready and cooldown states");++holes;
        }
        check(holes>350 && actual!=frame,"active icon drawn without an opaque plate");
    }
    RectangleStruct enlarged{button.X+5,button.Y+4,54,54},aperture{button.X+10,button.Y+9,44,44};
    background();auto untouchedButton=snapshot(button);drawPng(fixture,enlarged,0);auto fullIcon=snapshot(button);
    background();drawPng(fixture,enlarged,0,255,&aperture);auto croppedIcon=snapshot(button);
    for(int y=0;y<64;++y) for(int x=0;x<64;++x) {
        bool visible=x>=10 && x<54 && y>=9 && y<53;int i=y*64+x;
        check(croppedIcon[i]==(visible?fullIcon[i]:untouchedButton[i]),"icon clipping keeps enlarged scale and original background");
    }
    auto opaque=fixture;for(size_t i=3;i<opaque.pixels.size();i+=4) opaque.pixels[i]=255;
    cache("alpha-opaque.png",0,opaque);spec.icon="alpha-opaque.png";spec.name=L"主动技能";
    for(bool enabled:{true,false}) {
        background();ActiveHUD::drawButtonArt(spec,button,enabled);ActiveHUD::drawButtonLabel(spec,button,enabled);
        auto actual=snapshot(button);
        for(int y=0;y<64;++y) for(int x=0;x<64;++x) {
            // The shipped frame's exterior includes the bottom strip and its clipped top corners.
            bool exterior=x<6 || x>=58 || y<5 || y>=57 || (y<10 && (x<10 || x>=54));
            if(exterior) check(actual[y*64+x]==frame[y*64+x],"opaque icon and label stay inside frame silhouette");
        }
        check(actual!=frame,"opaque icon remains visible inside frame");
    }
    background();spec.icon="missing-alpha-icon.png";ActiveHUD::drawButtonArt(spec,button,false);auto fallback=snapshot(button);
    background();spec.icon="alpha-icon.png";ActiveHUD::drawButtonArt(spec,button,false);
    check(snapshot(button)==fallback,"fallback icon also preserves alpha during cooldown");
    auto original=Research::options[0];
    Research::options[0].icon="alpha-empty.png";Research::options[0].name.clear();Research::options[0].summary.clear();
    Layout l{};l.panel={view.X+290,view.Y+20,96,144};l.cardWidth=96;l.cardHeight=144;l.gap=16;
    auto native=l.card(0);
    constexpr double scale=0.65;constexpr int shift=13,opacity=180;
    int w=static_cast<int>(std::lround(native.Width*scale)),h=static_cast<int>(std::lround(native.Height*scale));
    RectangleStruct transformed{native.X+(native.Width-w)/2,native.Y+(native.Height-h)/2+shift,w,h};
    background();auto before=snapshot(transformed);
    drawAnimatedCard(l,0,0,{-1000,-1000},scale,shift,opacity,true);auto animated=snapshot(transformed);
    int transparent=0;
    for(int y=2;y<h/3;++y) for(int x=1;x<w/5;++x) {
        auto i=y*w+x;check(animated[i]==before[i],"animated transparent card reveals destination terrain");++transparent;
    }
    check(transparent>200 && animated!=before,"scaled card still draws nontransparent content");
    background();auto untouched=snapshot(view);
    drawAnimatedCard(l,0,0,{-1000,-1000},scale,shift,0,true);
    check(snapshot(view)==untouched,"invisible animated card restores original background");
    Research::options[0]=original;
    // Review the tested alpha levels over a patterned background.
    background();drawCardSkin(r);spec.icon="alpha-icon.png";
    ActiveHUD::drawButtonArt(spec,button,true);button.Y+=88;ActiveHUD::drawButtonArt(spec,button,false);
    Research::options[0].icon="alpha-empty.png";
    drawAnimatedCard(l,0,0,{-1000,-1000},scale,shift,opacity,true);
    Research::options[0]=original;
    capture({view.X,view.Y,460,250},"ui-alpha.ppm");
    Research::clearPresentationCaches();
    check(cardSkinCache().alpha.empty() && cardSkinCache().pixels.empty(),"card alpha cache cleared");
    std::ofstream("hex-icons.log",std::ios::app)<<"PASS active UI alpha in ready cooldown and fallback states\n"
        <<"PASS active icon and label stay inside frame\n"
        <<"PASS cached and animated card alpha\n";
}
}
