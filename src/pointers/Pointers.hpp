#pragma once
#include <d3d11.h>
#include <windows.h>

namespace NewBase
{
	using GetNativeHandler = void*(*)(void* table, std::uint64_t hash);

	struct PointerData
	{
		PVOID m_QueueDependency;

		PVOID m_SMPACreateStub;

		PVOID m_ReadGameConfig;
		PVOID m_GetPoolSize;
		PVOID m_CreatePool;
		PVOID m_GetPoolItem;

		PVOID m_OnProgramLoad;
		PVOID m_ResetThread;
		PVOID m_KillThread;
		PVOID m_ScriptVM;
		void* m_NativesTable;
		GetNativeHandler m_GetNativeHandler;

		PVOID m_FixVectors;
	};

	struct Pointers : PointerData
	{
		bool Init();
		bool InitScriptHook();
	};

	inline NewBase::Pointers Pointers;
}