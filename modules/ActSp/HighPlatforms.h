#pragma once

struct IStorage;

namespace HighPlatforms {
void tick();
void reset();
void forget(DWORD id);
void forgetEntry(DWORD id);
bool save(IStorage* storage);
bool load(IStorage* storage);
}
