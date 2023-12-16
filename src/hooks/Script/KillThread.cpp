#include "hooking/DetourHook.hpp"
#include "hooks/Hooks.hpp"
#include "jit/JIT.hpp"

namespace NewBase
{
	void Script::KillThread(rage::scrThread* thread)
	{
		JIT::JIT::UnregisterThread(thread);
		BaseHook::Get<Script::KillThread, DetourHook<decltype(&Script::KillThread)>>()->Original()(thread);
	}
}