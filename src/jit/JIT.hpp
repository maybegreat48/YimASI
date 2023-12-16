#pragma once
#include "Header.hpp"
#include <script/scrProgram.hpp>
#include <script/scrThread.hpp>
#include "ScriptFile.hpp"

namespace JIT
{
	class JIT
	{
		std::unordered_map<rage::scrProgram*, std::unique_ptr<ScriptFile>> Programs;
		std::unordered_map<rage::scrThread*, std::unique_ptr<Context>> Threads;

		ScriptFile* ScriptFromHash(std::uint32_t hash);

		void RegisterProgramImpl(rage::scrProgram* program, uint64_t* native_hashes);
		bool IsProgramRegisteredImpl(rage::scrProgram* program);
		void RegisterThreadImpl(rage::scrThread* thread);
		void UnregisterThreadImpl(rage::scrThread* thread);
		bool IsThreadRegisteredImpl(rage::scrThread* thread);
		void ScriptTickImpl(rage::scrProgram* program, rage::scrThread* thread, void* globals);

		static JIT& GetInstance()
		{
			static JIT instance;
			return instance;
		}

	public:

		static void RegisterProgram(rage::scrProgram* program, uint64_t* native_hashes)
		{
			GetInstance().RegisterProgramImpl(program, native_hashes);
		}

		static bool IsProgramRegistered(rage::scrProgram* program)
		{
			return GetInstance().IsProgramRegisteredImpl(program);
		}

		static void RegisterThread(rage::scrThread* thread)
		{
			GetInstance().RegisterThreadImpl(thread);
		}

		static void UnregisterThread(rage::scrThread* thread)
		{
			GetInstance().UnregisterThreadImpl(thread);
		}

		static bool IsThreadRegistered(rage::scrThread* thread)
		{
			return GetInstance().IsThreadRegisteredImpl(thread);
		}

		static void ScriptTick(rage::scrProgram* program, rage::scrThread* thread, void* globals)
		{
			GetInstance().ScriptTickImpl(program, thread, globals);
		}
	};
}