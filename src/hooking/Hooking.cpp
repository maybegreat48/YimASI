#include "Hooking.hpp"

#include "BaseHook.hpp"
#include "DetourHook.hpp"
#include "VMTHook.hpp"
#include "hooks/Hooks.hpp"
#include "pointers/Pointers.hpp"

namespace NewBase
{
	Hooking::Hooking()
	{
		// BaseHook::Add<Anticheat::QueueDependency>(new DetourHook("QueueDependency", Pointers.m_QueueDependency, Anticheat::QueueDependency));
		BaseHook::Add<Allocator::SMPACreateStub>(new DetourHook("SMPACreateStub", Pointers.m_SMPACreateStub, Allocator::SMPACreateStub));
		BaseHook::Add<GameFiles::ReadGameConfig>(new DetourHook("ReadGameConfig", Pointers.m_ReadGameConfig, GameFiles::ReadGameConfig));
		BaseHook::Add<Pools::GetPoolSize>(new DetourHook("GetPoolSize", Pointers.m_GetPoolSize, Pools::GetPoolSize));
		BaseHook::Add<Pools::CreatePool>(new DetourHook("CreatePool", Pointers.m_CreatePool, Pools::CreatePool));
		BaseHook::Add<Pools::GetPoolItem>(new DetourHook("GetPoolItem", Pointers.m_GetPoolItem, Pools::GetPoolItem));
		BaseHook::Add<Script::OnProgramLoad>(new DetourHook("OnProgramLoad", Pointers.m_OnProgramLoad, Script::OnProgramLoad));
		BaseHook::Add<Script::ResetThread>(new DetourHook("ResetThread", Pointers.m_ResetThread, Script::ResetThread));
		BaseHook::Add<Script::KillThread>(new DetourHook("KillThread", Pointers.m_KillThread, Script::KillThread));
		BaseHook::Add<Script::ScriptVM>(new DetourHook("ScriptVM", Pointers.m_ScriptVM, Script::ScriptVM));
	}

	Hooking::~Hooking()
	{
		DestroyImpl();
	}

	bool Hooking::Init()
	{
		return GetInstance().InitImpl();
	}

	void Hooking::Destroy()
	{
		GetInstance().DestroyImpl();
	}

	bool Hooking::InitImpl()
	{
		BaseHook::EnableAll();
		m_MinHook.ApplyQueued();

		return true;
	}

	void Hooking::DestroyImpl()
	{
		BaseHook::DisableAll();
		m_MinHook.ApplyQueued();

		for (auto it : BaseHook::Hooks())
		{
			delete it;
		}
		BaseHook::Hooks().clear();
	}
}