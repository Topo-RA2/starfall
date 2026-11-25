#pragma once

#include <Windows.h>

struct IStorage;
class TechnoClass;

namespace ActSp {
void reset();
void tick();
void forget(DWORD id);
void stopOrder(TechnoClass* unit);
bool save(IStorage* storage);
bool load(IStorage* storage);
}
