#include "Context.hpp"
#include "Utils.hpp"

#include <script/GtaThread.hpp>

using namespace asmjit::x86;

JIT::Context::Context(int stack_size) :
    StackSize(stack_size)
{
	StackMemory = new uint8_t[stack_size];
	memset(StackMemory, 0, stack_size);
}

void JIT::Context::SetMain(main_t main)
{
	MainFunction                                    = main;
	VmRegisterState.GPR[4]                          = ((uint64_t)(StackMemory) + StackSize); // RSP
	*(main_t*)((uint64_t)(StackMemory) + StackSize) = MainFunction;                 // return to main
}

void JIT::Context::SetThread(rage::scrThread* thread)
{
	Thread = thread;
}

void JIT::Context::SetStatics(void* statics)
{
	Statics = statics;
}

void JIT::Context::SetGlobals(void* globals)
{
	Globals = globals;
}

void JIT::Context::SetScriptFile(ScriptFile* file)
{
	File = file;
}

JIT::ScriptFile* JIT::Context::GetScriptFile()
{
	return File;
}

void JIT::Context::Call()
{
	GetVMEnter()(this);
}

void JIT::Context::SetupForceCleanup()
{
	ForceCleanupRegisterState = VmRegisterState;
	ForceCleanupSetup = false;
	ForceCleanupActive = false;
	ForceCleanupReturnAddress                                    = *reinterpret_cast<uint64_t*>(VmRegisterState.GPR[4]);
	reinterpret_cast<GtaThread*>(Thread)->m_force_cleanup_cause  = 0;
	reinterpret_cast<GtaThread*>(Thread)->m_force_cleanup_ip     = 0x1337;
	reinterpret_cast<GtaThread*>(Thread)->m_force_cleanup_filter = ForceCleanupFlags;
	LOG(INFO) << __FUNCTION__ ": setting up force cleanup for " << Thread->m_name << " with flags " << ForceCleanupFlags;
}

void JIT::Context::ForceCleanup()
{
	if (ForceCleanupRegisterState.GPR[4])
	{
		VmRegisterState = ForceCleanupRegisterState;
		*reinterpret_cast<uint64_t*>(VmRegisterState.GPR[4]) = ForceCleanupReturnAddress;
		ForceCleanupActive = true;
		LOG(INFO) << __FUNCTION__ ": dispatching force cleanup for " << Thread->m_name << " with flags " << ForceCleanupFlags;
	}
}

bool JIT::Context::NeedToSetupForceCleanup()
{
	return ForceCleanupSetup;
}

void JIT::Context::DebugRunScript()
{
	GetVMEnter();
	GetVMExit();

	while (true)
	{
		Call();

		if (SleepQueued)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(SleepMsecs));
			SleepQueued = false;
		}
		else if (KillThread)
		{
			return;
		}
	}
}

bool JIT::Context::IsKilled()
{
	bool val    = KillThread;
	KillThread = false;
	return val;
}

bool JIT::Context::IsYielding()
{
	bool val = SleepQueued;
	SleepQueued = false;
	return val;
}

int JIT::Context::GetYieldTimer()
{
	return SleepMsecs;
}

JIT::vmenter_t JIT::Context::GetVMEnter()
{
	static vmenter_t VmEnter = nullptr;

	if (VmEnter == nullptr)
	{
		asmjit::CodeHolder code;
		asmjit::StringLogger logger;

		code.init(g_JIT.environment(), g_JIT.cpuFeatures());
		code.setLogger(&logger);

		Assembler a(&code);

		a.mov(JIT::GetContextReg(), rcx); // move a1 to context
		for (int i = 1; i < 16; i++)
			a.mov(ptr(JIT::GetContextReg(), offsetof(Context, Context::CallerRegisterState) + i * 8), gpq(i)); // store caller registers
		for (int i = 1; i < 16; i++)
			a.mov(gpq(i), ptr(JIT::GetContextReg(), offsetof(Context, Context::VmRegisterState) + i * 8)); // load VM registers
		a.ret(); // return to VM main (or where we left off)

		g_JIT.add(&VmEnter, &code);
		LOG(INFO) << "GetVMEnter():\n" << logger.data();
	}

	return VmEnter;
}

JIT::vmexit_t JIT::Context::GetVMExit()
{
	static vmexit_t VmExit = nullptr;

	if (VmExit == nullptr)
	{
		asmjit::CodeHolder code;
		asmjit::StringLogger logger;

		code.init(g_JIT.environment(), g_JIT.cpuFeatures());
		code.setLogger(&logger);

		Assembler a(&code);

		for (int i = 1; i < 16; i++)
			a.mov(ptr(JIT::GetContextReg(), offsetof(Context, Context::VmRegisterState) + i * 8), gpq(i)); // store VM registers
		for (int i = 1; i < 16; i++)
			a.mov(gpq(i), ptr(JIT::GetContextReg(), offsetof(Context, Context::CallerRegisterState) + i * 8)); // load caller registers
		a.ret();                                                                              // return to caller

		g_JIT.add(&VmExit, &code);
		LOG(INFO) << "GetVMExit():\n" << logger.data();
	}

	return VmExit;
}

JIT::thrkill_t JIT::Context::GetThreadKill()
{
	static thrkill_t ThreadKill = nullptr;

	if (ThreadKill == nullptr)
	{
		asmjit::CodeHolder code;
		asmjit::StringLogger logger;

		code.init(g_JIT.environment(), g_JIT.cpuFeatures());
		code.setLogger(&logger);

		Assembler a(&code);

		a.mov(ptr(JIT::GetContextReg(), offsetof(Context, Context::KillThread), 1), 1);
		a.jmp(GetVMExit());

		g_JIT.add(&ThreadKill, &code);
		LOG(INFO) << "GetThreadKill():\n" << logger.data();
	}

	return ThreadKill;
}

JIT::Context::~Context()
{
	delete[] StackMemory;
}
