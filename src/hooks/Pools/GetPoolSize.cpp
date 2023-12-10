#include "hooking/DetourHook.hpp"
#include "hooks/Hooks.hpp"
#include "util/Joaat.hpp"

namespace NewBase
{
	static const std::unordered_map<uint32_t, unsigned int> s_PoolSizeOverrides = {
	    {"CEventNetwork"_J, 5000},      // allow 10x events to be stored
	    {"Decorator"_J, 1400},          // try to prevent decorator crashes
	    {"FragmentStore"_J, 50000},     // .yft files
	    {"fwScriptGuid"_J, 3072},       // increase script entity handles
	    {"HandlingData"_J, 5100},       // handling files
	    {"CPickupPlacement"_J, 280},    // map pickups
	    {"CEvent"_J, 4200},             // AI events, increasing this apparently fixes some crashes
	    {"CTaskSequenceList"_J, 250},   // task sequences, see above
	    {"AnimatedBuilding"_J, 1200},   //
	    {"Building"_J, 66000},          //
	    {"CCombatInfo"_J, 100},         // required by some mods?
	    {"ScenarioPoint"_J, 1800},      // map mods
	    {"ScenarioPointEntity"_J, 700}, // map mods
	    {"ScenarioPointWorld"_J, 2800}, // map mods
	    {"MaxNonRegionScenarioPointSpatialObjects"_J, 1800}, // map mods
	    {"Object"_J, 3000},                                  //
	};

	unsigned int Pools::GetPoolSize(rage::fwConfigManagerImpl<CGameConfig>* manager, uint32_t hash, int defaultValue)
	{
		g_LastPoolHash = hash;

		auto value = BaseHook::Get<Pools::GetPoolSize, DetourHook<decltype(&Pools::GetPoolSize)>>()->Original()(manager, hash, defaultValue);

		if (s_PoolSizeOverrides.count(hash))
		{
			LOG(VERBOSE) << __FUNCTION__ ": " << HEX(hash) << ": " << value << " -> " << s_PoolSizeOverrides.at(hash);
			return s_PoolSizeOverrides.at(hash);
		}

		return value;
	}
}