#include "Natives.hpp"
#include "Context.hpp"
#include "game/Crossmap.hpp"
#include <script/scrNativeHandler.hpp>
#include <script/types.hpp>

void TERMINATE_THIS_THREAD(rage::scrNativeCallContext* ctx)
{
	static bool test = __rdtsc();
	LOG(INFO) << __FUNCTION__ << ": " << test;
	return;
}

void WAIT(rage::scrNativeCallContext* ctx)
{
	static bool test = __rdtsc();
	LOG(INFO) << __FUNCTION__ << ": " << test;
	return;
}

namespace 
{
	// stub
	static void RETURN_FALSE(rage::scrNativeCallContext* ctx)
	{
		if (ctx->m_return_value)
			ctx->set_return_value<BOOL>(FALSE);
		return;
	}

	static std::unordered_map<rage::scrNativeHash, rage::scrNativeHandler> s_Natives = 
	{
	    {0x4EDE34FBADD967A6, WAIT},
	    {0x1090044AD1DA76FA, TERMINATE_THIS_THREAD},
	};
}

void JIT::FillNatives(rage::scrProgram* program, std::vector<uint64_t>& natives)
{
	auto native_table = new rage::scrNativeHandler[natives.size()]; // TODO: we never free this

	for (int i = 0; i < natives.size(); i++)
	{
		auto hash = NewBase::GetUntranslatedHash(natives[i]);
		if (auto it = s_Natives.find(hash); it != s_Natives.end())
			native_table[i] = it->second;
		else
			native_table[i] = RETURN_FALSE;
	}

	program->m_native_entrypoints = native_table;
}

void JIT::PatchNatives(rage::scrProgram* program, uint64_t* native_hashes)
{
	for (int i = 0; i < program->m_native_count; i++)
	{
		auto hash = NewBase::GetUntranslatedHash(native_hashes[i]);
		if (auto it = s_Natives.find(hash); it != s_Natives.end())
			program->m_native_entrypoints[i] = it->second;
	}
}
