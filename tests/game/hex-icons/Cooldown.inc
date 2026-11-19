namespace {
void testCooldown() {
    using namespace ActiveHUD;
    Group group{};
    for(auto [frames,seconds]:std::array<std::pair<int,int>,9>{{
        {0,0},{1,1},{14,1},{15,1},{16,2},{29,2},{30,2},{31,3},{INT_MAX,143165577}
    }}) {
        group.cooldownFrames=frames;
        check(cooldownSeconds(group)==seconds,"cooldown rounds up in game seconds without overflow");
    }
    group.cooldownFrames=0;
    for(int frames:{31,16,30,0}) trackCooldown(group,frames);
    check(cooldownSeconds(group)==2,"shared button shows earliest positive cooldown");
    group.ready.push_back(nullptr);
    check(!cooldownSeconds(group),"ready caster hides shared cooldown");

    auto surface=DSurface::Composite;auto view=DSurface::ViewBounds;
    RectangleStruct rect{view.X+16,view.Y+16,64,64};
    int background=color(49,91,39);
    surface->FillRect(&rect,background);auto blank=snapshot(rect);
    drawCooldown(group,rect);
    check(snapshot(rect)==blank,"ready button paints no countdown");
    group.ready.clear();group.cooldownFrames=0;drawCooldown(group,rect);
    check(snapshot(rect)==blank,"expired cooldown paints no zero");
    std::vector<WORD> one;
    for(int frames:{1,15,16,INT_MAX}) {
        surface->FillRect(&rect,background);group.cooldownFrames=frames;drawCooldown(group,rect);
        auto pixels=snapshot(rect);int changed=0;
        for(int y=0;y<64;++y) for(int x=0;x<64;++x) if(pixels[y*64+x]!=blank[y*64+x]) {
            ++changed;
            check(x>=11 && x<53 && y>=14 && y<39,"countdown fits inside frame above label");
        }
        check(changed>10 && changed<64*24/2,"countdown paints digits without an opaque plate");
        if(frames==1) one=pixels;
        else if(frames==15) check(pixels==one,"last second keeps displaying one");
        else if(frames==16) check(pixels!=one,"next second displays a different digit");
    }
    RectangleStruct sheet{view.X+8,view.Y+8,360,80};surface->FillRect(&sheet,background);
    ActiveSkills::Spec spec;spec.name=L"Cooldown";
    int slot=0;
    for(int frames:{0,1,16,150,1500}) {
        RectangleStruct button{sheet.X+4+slot++*buttonStride,sheet.Y+8,64,64};
        group.cooldownFrames=frames;
        drawButtonArt(spec,button,frames==0);drawCooldown(group,button);
        drawButtonLabel(spec,button,frames==0);
    }
    capture(sheet,"active-countdown.ppm");
    std::ofstream("hex-icons.log",std::ios::app)<<"PASS cooldown countdown rounding grouping and rendering\n";
}
}
