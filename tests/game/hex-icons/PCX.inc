namespace {
void testHexPCX() {
    auto surface=DSurface::Composite;
    const int columns=10,rows=(static_cast<int>(Research::options.size())+columns-1)/columns;
    RectangleStruct sheet{8,8,columns*64,rows*64};int background=color(14,23,35);
    surface->FillRect(&sheet,background);
    check(Research::options.size()==85,"complete HexBrawl icon set");
    for(size_t i=0;i<Research::options.size();++i) {
        const auto& option=Research::options[i];
        auto source=imageFor(option.icon,0,Research::Rarity::Common,true);
        auto path=Resources::pathKey("hx_"+option.icon.substr(option.icon.find_last_of("\\/")+1));
        path.replace(path.size()-4,4,".pcx");
        // Bypass all fallbacks: every actual PCX must load through the native engine.
        auto converted=imageFor(path,0,Research::Rarity::Common,false);
        check(!source->pixels.empty() && !converted->pixels.empty(),"PNG and PCX both loaded");
        check(source->width==96 && source->height==96 && converted->width==source->width &&
              converted->height==source->height,"PCX retains source dimensions");
        for(size_t p=3;p<source->pixels.size();p+=4)
            check(converted->pixels[p]==(source->pixels[p]>=128?255:0),"native PCX preserves transparent silhouette without opaque holes");
        auto pcxOption=option;pcxOption.icon=path;
        RectangleStruct tile{sheet.X+static_cast<int>(i%columns)*64,sheet.Y+static_cast<int>(i/columns)*64,64,64};
        drawProjectIcon(pcxOption,tile);
        auto pixels=snapshot(tile);
        check(pixels.front()==background && std::any_of(pixels.begin(),pixels.end(),[&](WORD p){return p!=background;}),
              "PCX research icon renders with transparent corner");
    }
    capture(sheet,"hex-icons-pcx.ppm");
    std::ofstream("hex-icons.log",std::ios::app)<<"PASS all HexBrawl PCX icons and transparency\n";
}
}
