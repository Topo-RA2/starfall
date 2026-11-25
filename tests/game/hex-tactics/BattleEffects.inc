namespace {
DWORD shieldTestId=0;
DWORD smokeTestId=0;
DWORD iceTestId=0;

AnimClass* battleEffect(const char* name,TechnoClass* owner=nullptr) {
    for(auto anim:AnimClass::Array) if(!anim->TimeToDie && !_stricmp(anim->Type->ID,name) && anim->OwnerObject==owner) return anim;
    return nullptr;
}

void snapshotBattleEffects(const CoordStruct& point) {
    auto surface=DSurface::Composite;
    const int width=std::min(620,surface->GetWidth()),height=std::min(360,surface->GetHeight());
    const auto center=TacticalClass::Instance->CoordsToClient(point).first;
    const int left=std::clamp(center.X-width/2,0,surface->GetWidth()-width);
    const int top=std::clamp(center.Y-height/2,0,surface->GetHeight()-height);
    auto pixels=static_cast<const BYTE*>(surface->Lock(0,0));
    if(!pixels) return;
    const int pitch=surface->GetPitch();
    std::ofstream output("hex-battle-effects.ppm",std::ios::binary);
    output<<"P6\n"<<width<<' '<<height<<"\n255\n";
    const int shifts[]={Drawing::RedShiftLeft,Drawing::GreenShiftLeft,Drawing::BlueShiftLeft};
    const int masks[]={255>>Drawing::RedShiftRight,255>>Drawing::GreenShiftRight,255>>Drawing::BlueShiftRight};
    for(int y=0;y<height;++y) {
        auto row=reinterpret_cast<const WORD*>(pixels+(top+y)*pitch)+left;
        for(int x=0;x<width;++x) for(int component=0;component<3;++component)
            output.put(static_cast<char>(((row[x]>>shifts[component])&masks[component])*255/masks[component]));
    }
    surface->Unlock();
}

void checkBattleEffects(int frame,bool loading,HouseClass* owner,HouseClass* foe) {
    if(!loading && frame==64) {
        for(const auto& [id,frames]:std::array<std::pair<const char*,int>,6>{{
            {"HXBLIND",16},{"HXPIERCE",16},{"HXSLOW",16},{"HXSHIELD",16},{"HXSHBLK",8},{"HXLEGACY",10}
        }}) {
            auto type=AnimTypeClass::Find(id);
            auto image=type?type->GetImage():nullptr;
            check(image && image->Frames==frames && image->GetData(),id);
        }
        unlock(owner,"sf_hx_freezingray",Research::Rarity::Rare);
        unlock(owner,"sf_hx_mirageshield",Research::Rarity::Common);
        unlock(owner,"sf_hx_legacy",Research::Rarity::Rare);
        auto shield=place("MGTK",owner,{64,76});shieldTestId=shield->UniqueID;
        place("MGTK",owner,{66,78});
        settle();
        check(battleEffect("HXSHIELD",shield)!=nullptr,"shield is visible while charged");
        auto point=shield->Location;
        MapClass::Instance.RevealArea2(&point,10,owner,0,0,0,1,0);
        auto position=TacticalClass::CoordsToScreen(point);
        position.X-=DSurface::Composite->GetWidth()/2;
        position.Y-=DSurface::Composite->GetHeight()/2;
        TacticalClass::Instance->SetTacticalPosition(&position);
    }
    if(!loading && frame==70) {
        auto shield=find(shieldTestId);
        auto prism=static_cast<FootClass*>(place("SREF",owner,{67,75}));prism->SightTimer.Start(0);
        auto ice=place("HTNK",foe,{65,74});iceTestId=ice->UniqueID;
        auto smoke=place("HTNK",foe,{62,77});smokeTestId=smoke->UniqueID;
        auto pierced=place("HTNK",foe,{63,74});
        auto flak=place("FLAKT",owner,{63,78}),ggi=place("GGI",owner,{64,73});
        unlock(owner,"sf_hx_smokeshell",Research::Rarity::Rare);
        unlock(owner,"sf_hx_armorpiercing",Research::Rarity::Rare);
        hit(ice,prism,20);
        hit(smoke,flak,20);
        hit(pierced,ggi,20);
        check(battleEffect("HXSLOW",ice) && battleEffect("HXBLIND",smoke) && battleEffect("HXPIERCE",pierced),
            "cold smoke and piercing effects are on the targets");
        const int hp=shield->Health;
        int damage=20;
        shield->ReceiveDamage(&damage,0,WarheadTypeClass::Find("AP"),pierced,false,false,foe);
        settle();
        check(shield->Health==hp && battleEffect("HXSHBLK",shield) && !battleEffect("HXSHIELD",shield),
            "shield blocks and flashes then hides during cooldown");
        auto conscript=place("E2",owner,{61,76});
        const auto deathCell=conscript->GetMapCoords();
        hit(conscript,pierced);
        auto legacy=battleEffect("HXLEGACY");
        check(legacy && CellClass::Coord2Cell(legacy->Location)==deathCell,"legacy pulse stays at the death cell");
        std::ofstream("shield-vfx.id")<<shieldTestId;
    }
    if(!loading && frame==74) snapshotBattleEffects(find(shieldTestId)->Location);
    if(!loading && frame==100) {
        check(!battleEffect("HXSLOW",find(iceTestId)) && !battleEffect("HXBLIND",find(smokeTestId)),
            "cold and smoke animations expire with the debuffs");
        check(!battleEffect("HXLEGACY") && !battleEffect("HXSHBLK",find(shieldTestId)),"one-shot clips finish");
    }
    if(frame==144 || frame==145) {
        if(loading) std::ifstream("shield-vfx.id")>>shieldTestId;
        auto tank=find(shieldTestId);
        check(tank && (battleEffect("HXSHIELD",tank)!=nullptr)==(frame==145),"shield visuals follow saved cooldown boundary");
    }
}
}
