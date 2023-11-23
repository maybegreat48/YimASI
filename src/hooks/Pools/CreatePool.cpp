#include "hooking/DetourHook.hpp"
#include "hooks/Hooks.hpp"
#include "util/Joaat.hpp"

namespace NewBase
{
	void* Pools::CreatePool(void* pool, int size, const char* name, int unk1, int unk2, bool unk3)
	{
		BaseHook::Get<Pools::CreatePool, DetourHook<decltype(&Pools::CreatePool)>>()->Original()(pool, size, name, unk1, unk2, unk3);
		g_Pools.insert({pool, g_LastPoolHash});
		return pool;
	}
}