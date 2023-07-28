#pragma once
#include <d3d11.h>
#include <windows.h>

namespace NewBase
{
	struct PointerData
	{
	};

	struct Pointers : PointerData
	{
		bool Init();
		bool InitScriptHook();
	};

	inline NewBase::Pointers Pointers;
}