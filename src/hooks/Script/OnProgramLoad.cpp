#include "hooking/DetourHook.hpp"
#include "hooks/Hooks.hpp"
#include "jit/JIT.hpp"

namespace NewBase
{
	void Script::OnProgramLoad(rage::scrProgram* program)
	{
		auto hashes = std::make_unique<uint64_t[]>(program->m_native_count);
		memcpy(hashes.get(), program->m_native_entrypoints, program->m_native_count * 8);
		BaseHook::Get<Script::OnProgramLoad, DetourHook<decltype(&Script::OnProgramLoad)>>()->Original()(program);
		JIT::JIT::RegisterProgram(program, hashes.get());
	}
}