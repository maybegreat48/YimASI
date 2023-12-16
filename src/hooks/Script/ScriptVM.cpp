#include "hooking/DetourHook.hpp"
#include "hooks/Hooks.hpp"
#include "jit/JIT.hpp"

namespace NewBase
{
	rage::eThreadState Script::ScriptVM(void* stack, void** globals, rage::scrProgram* program, rage::scrThreadContext* context)
	{
		if (JIT::JIT::IsThreadRegistered(rage::tlsContext::get()->m_script_thread))
		{
			JIT::JIT::ScriptTick(program, rage::tlsContext::get()->m_script_thread, globals);
			return rage::tlsContext::get()->m_script_thread->m_context.m_state;
		}

		return BaseHook::Get<Script::ScriptVM, DetourHook<decltype(&Script::ScriptVM)>>()->Original()(stack, globals, program, context);
	}
}