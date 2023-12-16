#pragma once
#include <asmjit.h>

namespace JIT
{
	constexpr bool ALLOCATE_SHADOW_SPACE = true;
	constexpr bool CRASH_ON_OOB_ARRAY_ACCESS = true;

	inline asmjit::JitRuntime g_JIT{};

	using namespace asmjit::x86;
}