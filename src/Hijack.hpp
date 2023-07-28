#pragma once
#include "hooking/IATHook.hpp"

namespace NewBase
{
	class Hijack
	{
		static BOOL SpinCountHookTarget(void* a1, int a2);
		static void LoadOriginalImports();

		static inline std::function<void()> m_MainFunc;
		static inline std::unique_ptr<IATHook<decltype(&SpinCountHookTarget)>> m_Hook;

	public:
		static void Init(std::function<void()> main_func);
	};
}