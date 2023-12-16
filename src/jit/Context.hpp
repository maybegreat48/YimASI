#pragma once
#include <script/scrNativeHandler.hpp>
#include "Header.hpp"

namespace rage
{
	class scrThread;
}

namespace JIT
{
	class ScriptFile;

	// TODO: xmm regs?
	struct RegisterState
	{
		uint64_t GPR[16]{};
	};

	using main_t = void (*)();
	class Context;
	using vmenter_t = void (*)(Context*);
	using vmexit_t  = void (*)();
	using thrkill_t = void (*)();

	// runtime context that is always stored in RAX

#pragma pack(push, 0x10)
	class Context
	{
		RegisterState VmRegisterState{};
		RegisterState CallerRegisterState{};
		RegisterState ForceCleanupRegisterState{};
		void* Globals{};
		void* Statics{};
		ScriptFile* File{};
		bool SleepQueued{};
		int SleepMsecs{};
		rage::scrThread* Thread{};
		void* StackMemory{};
		int StackSize{};
		bool NeedToSetupForceCleanup{};
		bool ForceCleanupSetupDone{};
		bool KillThread{};
		rage::scrNativeCallContext CallContext{};

		main_t MainFunction{};
		

	public:
		Context(int stack_size = 0x75300);
		void SetMain(main_t main);
		void SetThread(rage::scrThread* thread);
		void SetStatics(void* statics);
		void SetGlobals(void* globals);
		void SetScriptFile(ScriptFile* file);
		ScriptFile* GetScriptFile();

		void Call();
		void ForceCleanup();
		void DebugRunScript();

		bool IsKilled();
		bool IsYielding();
		int GetYieldTimer();

		static vmenter_t GetVMEnter();
		static vmexit_t GetVMExit();
		static thrkill_t GetThreadKill();

		static size_t GetSize(int num_natives);

		static Context& GetFromCallCtx(rage::scrNativeCallContext* ctx);

		~Context();

		friend class Function;
	};
#pragma pack(pop)
}