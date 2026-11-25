#include "CarrierConversion.h"
#include <Kamikaze.h>
#include <SpawnManagerClass.h>

namespace CarrierConversion {
bool supported(const TechnoClass* carrier) {
    const auto manager=carrier->SpawnManager;
    return !manager || (manager->Owner==carrier && manager->SpawnCount==manager->SpawnedNodes.Count);
}

bool reserveSlots(TechnoClass* carrier,const TechnoTypeClass* destination) {
    auto manager=carrier->SpawnManager;
    return !manager || manager->SpawnType!=destination->Spawns || destination->SpawnsNumber<=manager->SpawnCount ||
        manager->SpawnedNodes.Reserve(destination->SpawnsNumber);
}

void apply(TechnoClass* carrier,const TechnoTypeClass* destination) {
    const int capacity=destination->Spawns ? std::max(0,destination->SpawnsNumber) : 0;
    auto& manager=carrier->SpawnManager;
    if(manager) {
        auto& nodes=manager->SpawnedNodes;
        const int keep=manager->SpawnType==destination->Spawns ? capacity : 0;
        while(nodes.Count>keep) {
            auto node=nodes[nodes.Count-1];
            nodes.RemoveItem(nodes.Count-1);
            manager->SpawnCount=nodes.Count;
            // Unlink before UnInit broadcasts removal. Already launched, detached
            // missiles belong to the engine and no longer occupy a slot.
            if(auto child=node->Unit) {
                child->SpawnOwner=nullptr;
                Kamikaze::Instance.Remove(child);
                child->UnInit();
            }
            GameDelete(node);
        }
        if(!keep) {
            GameDelete(manager);
            manager=nullptr;
        }
    }
    if(!capacity) return;
    if(!manager) {
        // Construction and regeneration run the installed engine/Ares hooks,
        // including classification and initialization of custom missiles.
        // Enter the native constructor directly. The SDK's delegating wrapper
        // initializes C++ members before its thiscall jump and loses this on MSVC.
        using Construct=SpawnManagerClass*(__thiscall*)(void*,TechnoClass*,AircraftTypeClass*,int,int,int);
        auto storage=YRMemory::AllocateChecked(sizeof(SpawnManagerClass));
        manager=reinterpret_cast<Construct>(0x6B6C90)(storage,carrier,destination->Spawns,capacity,
            destination->SpawnRegenRate,destination->SpawnReloadRate);
        return;
    }
    manager->RegenRate=destination->SpawnRegenRate;
    manager->ReloadRate=destination->SpawnReloadRate;
    while(manager->SpawnCount<capacity) {
        auto node=GameCreate<SpawnControl>();
        node->Status=SpawnNodeStatus::Dead;
        node->SpawnTimer.Start(manager->RegenRate);
        // Storage was reserved before committing the type change.
        manager->SpawnedNodes.AddItem(node);
        manager->SpawnCount=manager->SpawnedNodes.Count;
    }
}
}
