#include "JIT.hpp"
#include "util/Joaat.hpp"
#include "Natives.hpp"

JIT::ScriptFile* JIT::JIT::ScriptFromHash(std::uint32_t hash)
{
	for (auto& p : Programs)
	{
		if (p.first->m_name_hash == hash)
			return p.second.get();
	}

	return nullptr;
}

void JIT::JIT::RegisterProgramImpl(rage::scrProgram* program, uint64_t* native_hashes)
{
	LOG(VERBOSE) << __FUNCTION__ ": " << HEX(program->m_name_hash) << ", " << program->m_name;

	// debug
	if (program->m_name_hash != NewBase::Joaat("tennis"))
		return;

	if (!IsProgramRegistered(program))
	{
		PatchNatives(program, native_hashes);

		auto file = std::make_unique<ScriptFile>(program);
		file->OutlineProgram();
		file->AssembleProgram();
		Programs.insert({program, std::move(file)});
	}
}

bool JIT::JIT::IsProgramRegisteredImpl(rage::scrProgram* program)
{
	return Programs.contains(program);
}

void JIT::JIT::RegisterThreadImpl(rage::scrThread* thread)
{
	if (!IsThreadRegistered(thread) && ScriptFromHash(thread->m_context.m_script_hash))
	{
		LOG(VERBOSE) << __FUNCTION__;

		auto context = std::make_unique<Context>();
		context->SetThread(thread);
		context->SetMain(ScriptFromHash(thread->m_context.m_script_hash)->GetMainFunction());
		Threads.insert({thread, std::move(context)});

		//MessageBoxA(0, "A", "B", 0);
	}
}

void JIT::JIT::UnregisterThreadImpl(rage::scrThread* thread)
{
	std::erase_if(Threads, [thread](const auto& entry) {
		return entry.first == thread;
	});
}

bool JIT::JIT::IsThreadRegisteredImpl(rage::scrThread* thread)
{
	return Threads.contains(thread);
}

void JIT::JIT::ScriptTickImpl(rage::scrProgram* program, rage::scrThread* thread, void* globals)
{
	LOG(VERBOSE) << __FUNCTION__;

	auto ctx = Threads.at(thread).get();
	ctx->SetStatics(thread->m_stack);
	ctx->SetGlobals(globals);

	ctx->Call();

	if (ctx->IsKilled())
		thread->m_context.m_state = rage::eThreadState::killed;
	else if (ctx->IsYielding())
	{
		thread->m_context.m_state = rage::eThreadState::idle;
	}
}
