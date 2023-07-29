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
		BaseHook::Add<Anticheat::QueueDependency>(new DetourHook("QueueDependency", Pointers.m_QueueDependency, Anticheat::QueueDependency));
		BaseHook::Add<Allocator::SMPACreateStub>(new DetourHook("SMPACreateStub", Pointers.m_SMPACreateStub, Allocator::SMPACreateStub));
		BaseHook::Add<GameFiles::ReadGameConfig>(new DetourHook("ReadGameConfig", Pointers.m_ReadGameConfig, GameFiles::ReadGameConfig));
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