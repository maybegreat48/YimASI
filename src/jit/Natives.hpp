#pragma once
#include <script/scrProgram.hpp>
#include "Header.hpp"

void TERMINATE_THIS_THREAD(rage::scrNativeCallContext* ctx);
void WAIT(rage::scrNativeCallContext* ctx);
void HAS_FORCE_CLEANUP_OCCURRED(rage::scrNativeCallContext* ctx);
void NETWORK_SET_RICH_PRESENCE(rage::scrNativeCallContext* ctx);

namespace JIT
{
	/// <summary>
	/// For debug use only
	/// </summary>
	void FillNatives(rage::scrProgram* program, std::vector<uint64_t>& natives);

	void PatchNatives(rage::scrProgram* program, uint64_t* native_hashes);

	bool NeedToFixVecRefrs(rage::scrNativeHandler handler);
}