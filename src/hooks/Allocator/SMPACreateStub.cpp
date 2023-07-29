#include "hooking/DetourHook.hpp"
#include "hooks/Hooks.hpp"

#include <rage/sysMemAllocator.hpp>
#include <script/tlsContext.hpp>

namespace NewBase
{
	// from https://github.com/citizenfx/fivem/blob/7672c8c849165dad70a1e82f89e31059d8fcf20d/code/components/gta-streaming-five/src/UnkStuff.cpp#L159
	void* Allocator::SMPACreateStub(void* a1, void* a2, size_t size, void* a4, bool a5)
	{
		if (size == 0xD00000)
		{
			// free original allocation
			rage::tlsContext::get()->m_allocator->Free(a2);

			size = 0x1200000;
			a2   = rage::tlsContext::get()->m_allocator->Allocate(size, 16, 0);
		}

		return BaseHook::Get<Allocator::SMPACreateStub, DetourHook<decltype(&Allocator::SMPACreateStub)>>()->Original()(a1, a2, size, a4, a5);
	}
}