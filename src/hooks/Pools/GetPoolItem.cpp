#include "hooking/DetourHook.hpp"
#include "hooks/Hooks.hpp"
#include "util/Joaat.hpp"

namespace NewBase
{
	void* Pools::GetPoolItem(void* pool)
	{
		auto item = BaseHook::Get<Pools::GetPoolItem, DetourHook<decltype(&Pools::GetPoolItem)>>()->Original()(pool);

		if (item)
			return item;

		if (g_Pools.count(pool))
		{
			auto hash = g_Pools[pool];
			std::ostringstream message;
			message << "ERROR: POOL 0x" << std::hex << std::uppercase << hash << " FULL!";
			MessageBoxA(0, message.str().c_str(), "YimASI", MB_ICONSTOP | MB_SYSTEMMODAL | MB_TOPMOST | MB_SETFOREGROUND); // inspired by FiveM
		}

		return nullptr;
	}
}