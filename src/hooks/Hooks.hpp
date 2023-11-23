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

	namespace Pools
	{
		extern unsigned int GetPoolSize(rage::fwConfigManagerImpl<CGameConfig>* mgr, uint32_t hash, int defaultValue);
		extern void* CreatePool(void* pool, int size, const char* name, int unk1, int unk2, bool unk3);
		extern void* GetPoolItem(void* pool);
	}
}