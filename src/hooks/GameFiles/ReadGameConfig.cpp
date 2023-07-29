#include "hooking/DetourHook.hpp"
#include "hooks/Hooks.hpp"
#include "util/Joaat.hpp"

#include <rage/sysMemAllocator.hpp>
#include <script/tlsContext.hpp>

namespace NewBase
{
	rage::fwConfigManagerImpl<CGameConfig>* GameFiles::ReadGameConfig(rage::fwConfigManagerImpl<CGameConfig>* manager, const char* file)
	{
		auto ret = BaseHook::Get<GameFiles::ReadGameConfig, DetourHook<decltype(&GameFiles::ReadGameConfig)>>()->Original()(manager, file);

		// increase pools

		manager->m_config->m_pool_sizes.append({(char*)"CEventNetwork", 5000}); // allow 10x events to be stored
		manager->m_config->m_pool_sizes.append({(char*)"Decorator", 1400});     // try to prevent decorator crashes

		return ret;
	}
}