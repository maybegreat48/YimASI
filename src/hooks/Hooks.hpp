#pragma once
#include <d3d11.h>
#include <game_files/CGameConfig.hpp>

namespace NewBase
{
	namespace Anticheat
	{
		extern void QueueDependency(void* dependency);
	}

	namespace Allocator
	{
		extern void* SMPACreateStub(void* a1, void* a2, size_t size, void* a4, bool a5);
	}

	namespace GameFiles
	{
		extern rage::fwConfigManagerImpl<CGameConfig>* ReadGameConfig(rage::fwConfigManagerImpl<CGameConfig>* manager, const char* file);
	}
}