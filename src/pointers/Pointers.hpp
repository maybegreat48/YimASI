#pragma once
#include <d3d11.h>
#include <windows.h>

namespace NewBase
{
	struct PointerData
	{
		PVOID m_QueueDependency;
		PVOID m_SMPACreateStub;
		PVOID m_ReadGameConfig;
		PVOID m_GetPoolSize;
		PVOID m_CreatePool;
		PVOID m_GetPoolItem;
	};

	struct Pointers : PointerData
	{
		bool Init();
		bool InitScriptHook();
	};

	inline NewBase::Pointers Pointers;
}