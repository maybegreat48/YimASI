#include "hooking/DetourHook.hpp"
#include "hooks/Hooks.hpp"
#include "jit/JIT.hpp"

namespace NewBase
{
	void Script::ResetThread(rage::scrThread* thread, int hash, void* arguments, int count)
	{
		BaseHook::Get<Script::ResetThread, DetourHook<decltype(&Script::ResetThread)>>()->Original()(thread, hash, arguments, count);
		JIT::JIT::RegisterThread(thread);
	}
}