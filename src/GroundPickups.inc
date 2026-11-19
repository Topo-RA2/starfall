bool hasNativeCrate(const CellClass* cell) {
    auto overlay=OverlayTypeClass::Array.GetItemOrDefault(cell->OverlayTypeIndex);
    return overlay && overlay->Crate;
}

// Called on the final cell returned by the engine's targeted crate search.
bool redirectCrateFromPickup(CellStruct& at) {
    if(!groundPickups.contains({at.X,at.Y})) return true;
    auto& map=MapClass::Instance;
    auto origin=map.TryGetCellAt(at);
    if(!origin) return false;
    const auto speed=origin->LandType==LandType::Water?SpeedType::Float:SpeedType::Track;
    for(int radius=1;radius<=3;++radius) {
        for(int y=-radius;y<=radius;++y) for(int x=-radius;x<=radius;++x) {
            if(std::max(std::abs(x),std::abs(y))!=radius) continue;
            CellStruct candidate{short(at.X+x),short(at.Y+y)};
            if(!map.IsWithinUsableArea(candidate,true) || groundPickups.contains({candidate.X,candidate.Y})) continue;
            auto cell=map.TryGetCellAt(candidate);
            if(!cell || cell->OverlayTypeIndex!=-1 || cell->GetBuilding()) continue;
            const auto usable=map.NearByLocation(candidate,speed,-1,MovementZone::Normal,false,1,1,false,false,false,true,CellStruct::Empty,false,false);
            if(usable!=candidate) continue;
            at=candidate;
            return true;
        }
    }
    return false;
}

bool placePickup(const string& id,CellStruct origin) {
    auto& map=MapClass::Instance;
    if(!pickupTypes.contains(id) || !map.IsWithinUsableArea(origin,false)) return false;
    for(int radius=0;radius<=3;++radius) {
        for(int y=-radius;y<=radius;++y) for(int x=-radius;x<=radius;++x) {
            if(std::max(std::abs(x),std::abs(y))!=radius) continue;
            CellStruct at{short(origin.X+x),short(origin.Y+y)};
            if(!map.IsWithinUsableArea(at,false) || groundPickups.contains({at.X,at.Y})) continue;
            auto cell=map.TryGetCellAt(at);
            if(!cell || hasNativeCrate(cell) || cell->GetBuilding() || cell->LandType==LandType::Wall ||
               cell->LandType==LandType::Rock || cell->LandType==LandType::None) continue;
            groundPickups.emplace(PickupCell{at.X,at.Y},GroundPickup{id,static_cast<unsigned>(Unsorted::CurrentFrame)});
            cell->MarkForRedraw();
            return true;
        }
    }
    return false;
}

void collectPickups() {
    for(auto it=groundPickups.begin();it!=groundPickups.end();) {
        auto cell=MapClass::Instance.TryGetCellAt(CellStruct{it->first.first,it->first.second});
        auto type=pickupTypes.find(it->second.type);
        const unsigned age=static_cast<unsigned>(Unsorted::CurrentFrame)-it->second.born;
        if(cell && type!=pickupTypes.end() && (type->second.lifetime<0 || age<static_cast<unsigned>(type->second.lifetime))) {
            TechnoClass* collector=nullptr;
            for(auto object=cell->GetContent();object;object=object->NextObject) {
                if(object->WhatAmI()!=AbstractType::Unit && object->WhatAmI()!=AbstractType::Infantry) continue;
                auto unit=static_cast<TechnoClass*>(object);
                if(!alive(unit) || !unit->IsOnMap || unit->InLimbo || unit->Transporter ||
                   unit->IsSinking || unit->IsBeingWarpedOut() || unit->IsInAir() ||
                   unit->GetCell()!=cell || !unit->Owner) continue;
                if(!collector || unit->UniqueID<collector->UniqueID) collector=unit;
            }
            if(!collector) {++it;continue;}
            auto event=eventSnapshot("onpickup",collector,collector,collector,collector,0);
            event.snapshot={it->second.type};
            event.hasImpact=true;event.impact=cell->GetCoordsWithBridge();
            pending.push_back(std::move(event));
        }
        if(cell) cell->MarkForRedraw();
        it=groundPickups.erase(it);
    }
}

SHPStruct* pickupImage(const string& name) {
    auto [it,added]=pickupImages.try_emplace(name);
    if(added) {
        CCFileClass file(name.c_str());
        if(file.Exists()) it->second=std::make_unique<SHPReference>(name.c_str());
    }
    return it->second?it->second->GetData():nullptr;
}

SHPStruct* resolvePickupImage(const string& name) {
    if(!name.empty()) {
        auto image=pickupImage(name);
        if(image && image->Frames>0) return image;
    }
    // The theater may provide WCRATE as a theater-specific SHP.
    auto type=OverlayTypeClass::Find("WCRATE");
    if(auto image=type?type->GetImage():nullptr) return image->GetData();
    return pickupImage("wcrate.shp");
}
void drawGroundPickups() {
    if(groundPickups.empty() || !DSurface::Temp || !TacticalClass::Instance) return;
    auto bounds=DSurface::Temp->GetRect();
    for(const auto& [at,pickup]:groundPickups) {
        auto cell=MapClass::Instance.TryGetCellAt(CellStruct{at.first,at.second});
        auto type=pickupTypes.find(pickup.type);
        if(!cell || type==pickupTypes.end() || cell->IsShrouded() || cell->IsFogged()) continue;
        auto [pt,visible]=TacticalClass::Instance->CoordsToClient(cell->GetCoordsWithBridge());
        if(!visible) continue;
        auto image=resolvePickupImage(type->second.image);
        if(!image || image->Frames<=0) continue;
        const unsigned age=static_cast<unsigned>(Unsorted::CurrentFrame)-pickup.born;
        TacticalClass::Instance->RegisterDirtyArea({pt.X-image->Width/2,pt.Y-image->Height/2,image->Width,image->Height},false);
        DSurface::Temp->DrawSHP(FileSystem::UNITx_PAL,image,(age/4)%image->Frames,&pt,&bounds,
            BlitterFlags::Centered,0,0,ZGradient::Ground,1000,0,nullptr,0,0,0);
    }
}
