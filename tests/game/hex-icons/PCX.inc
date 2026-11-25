namespace {
void testHexPCX() {
    auto surface=DSurface::Composite;
    char entries[32768]{};
    auto count=GetPrivateProfileSectionA("Starfall.ResearchTypes",entries,sizeof(entries),".\\Starfall.ini");
    check(count && count<sizeof(entries)-2,"read independent project registry");
    std::set<std::string> registered,loaded;
    for(char* entry=entries;*entry;entry+=strlen(entry)+1) {
        auto value=strchr(entry,'=');
        check(value && value[1],"research registry entry has an ID");
        registered.insert(Resources::pathKey(value+1));
    }
    for(const auto& option:Research::options) loaded.insert(option.id);
    check(registered==loaded,"complete HexBrawl icon set");
    const int columns=10,rows=(static_cast<int>(loaded.size())+columns-1)/columns;
    const int tileSize=std::min({64,(surface->GetWidth()-8)/columns,(surface->GetHeight()-8)/rows});
    check(tileSize>0,"icon sheet fits the surface");
    RectangleStruct sheet{8,8,columns*tileSize,rows*tileSize};int background=color(14,23,35);
    surface->FillRect(&sheet,background);
    std::set<std::string> iconPaths;
    for(size_t i=0;i<Research::options.size();++i) {
        const auto& option=Research::options[i];
        const auto path=Resources::pathKey(option.icon);
        check(path.starts_with("hx_") && path.ends_with(".pcx") && path.find_first_of("\\/")==std::string::npos &&
              iconPaths.insert(path).second,"unique root-level PCX icon");
        auto icon=imageFor(path,0,Research::Rarity::Common,false);
        check(icon->width==96 && icon->height==96 && icon->pixels.size()==96*96*4,"native PCX loads without fallback");
        RectangleStruct tile{sheet.X+static_cast<int>(i%columns)*tileSize,sheet.Y+static_cast<int>(i/columns)*tileSize,tileSize,tileSize};
        drawProjectIcon(option,tile);
        auto pixels=snapshot(tile);
        check(pixels.front()==background && std::any_of(pixels.begin(),pixels.end(),[&](WORD p){return p!=background;}),
              "PCX research icon renders with transparent corner");
    }
    capture(sheet,"hex-icons-pcx.ppm");
    std::ofstream("hex-icons.log",std::ios::app)<<"PASS all HexBrawl PCX icons and transparency\n";
}
}
