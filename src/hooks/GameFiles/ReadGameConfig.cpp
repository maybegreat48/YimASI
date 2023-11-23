#include "hooking/DetourHook.hpp"
#include "hooks/Hooks.hpp"
#include "util/Joaat.hpp"

#include <rage/sysMemAllocator.hpp>
#include <script/tlsContext.hpp>

namespace NewBase
{
	void ModifyPoolSize(rage::atArray<CPoolSize>& sizes, const char* poolName, unsigned int size)
	{
		for (auto& entry : sizes)
		{
			if (!stricmp(entry.m_pool, poolName))
			{
				entry.m_size = size;
				return;
			}
		}

		sizes.append({(char*)poolName, size});
	}

	rage::fwConfigManagerImpl<CGameConfig>* GameFiles::ReadGameConfig(rage::fwConfigManagerImpl<CGameConfig>* manager, const char* file)
	{
		auto ret = BaseHook::Get<GameFiles::ReadGameConfig, DetourHook<decltype(&GameFiles::ReadGameConfig)>>()->Original()(manager, file);

		// increase pools
		// https://github.com/pnwparksfan/gameconfig/blob/master/versions/latest/gameconfig.xml

		// too late to modify this here
		#if 0
		ModifyPoolSize(manager->m_config->m_pool_sizes, "CEventNetwork", 5000); // allow 10x events to be stored
		ModifyPoolSize(manager->m_config->m_pool_sizes, "Decorator", 1400);     // try to prevent decorator crashes
		ModifyPoolSize(manager->m_config->m_pool_sizes, "FragmentStore", 285800); // .yft files
		ModifyPoolSize(manager->m_config->m_pool_sizes, "fwScriptGuid", 3072);    // increase script entity handles
		ModifyPoolSize(manager->m_config->m_pool_sizes, "HandlingData", 5100);    // handling files
		ModifyPoolSize(manager->m_config->m_pool_sizes, "CPickupPlacement", 280);    // map pickups
		ModifyPoolSize(manager->m_config->m_pool_sizes, "CEvent", 4200); // AI events, increasing this apparently fixes some crashes
		ModifyPoolSize(manager->m_config->m_pool_sizes, "CTaskSequenceList", 250); // task sequences, see above
		ModifyPoolSize(manager->m_config->m_pool_sizes, "AnimatedBuilding", 1200);
		ModifyPoolSize(manager->m_config->m_pool_sizes, "Building", 66000);
		ModifyPoolSize(manager->m_config->m_pool_sizes, "CCombatInfo", 100); // required by some mods?
		ModifyPoolSize(manager->m_config->m_pool_sizes, "ScenarioPoint", 1800); // map mods
		ModifyPoolSize(manager->m_config->m_pool_sizes, "ScenarioPointEntity", 700); // map mods
		ModifyPoolSize(manager->m_config->m_pool_sizes, "ScenarioPointWorld", 2800); // map mods
		ModifyPoolSize(manager->m_config->m_pool_sizes, "MaxNonRegionScenarioPointSpatialObjects", 1800); // map mods
		ModifyPoolSize(manager->m_config->m_pool_sizes, "Object", 3000);
		#endif


		return ret;
	}
}