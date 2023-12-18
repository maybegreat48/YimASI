#include "Natives.hpp"
#include "Context.hpp"
#include "pointers/Pointers.hpp"
#include "game/Crossmap.hpp"
#include <script/scrNativeHandler.hpp>
#include <script/types.hpp>
#include <unordered_set>

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

void HAS_FORCE_CLEANUP_OCCURRED(rage::scrNativeCallContext* ctx)
{
	static bool test = __rdtsc();
	LOG(INFO) << __FUNCTION__ << ": " << test;
	return;
}

void NETWORK_SET_RICH_PRESENCE(rage::scrNativeCallContext* ctx)
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
	    {0xC968670BFACE42D9, HAS_FORCE_CLEANUP_OCCURRED},
	    {0x1DCCACDCFC569362, NETWORK_SET_RICH_PRESENCE},
	};

	static const std::unordered_set<rage::scrNativeHash> s_OutVectorNatives = 
	{
		0xECB2FC7235A7D137, 
		0x352A9F6BCF90081F,
		0x252BDC06B73FA6EA,
		0x8BDC7BFC57A81E76,
		0xA4A0065E39C9F25C,
		0x03E8D3D5F549087A,
		0x8D7A43EC6A5FEA45,
		0xDFB4138EEFED7B81,
		0x82FDE6A57EE4EE44,
		0x6874E2190B0C1972,
		0x1CEFB61F193070AE,
		0x584FDFDA48805B86,
		0x371EA43692861CF1,
		0x163F8B586BC95F2A,
		0xB61C8E878A4199CA,
		0x240A18690AE96513,
		0x2EABE3B06F58C1BE,
		0xFF071FB798B803B0,
		0xE50E52416CCF948B,
		0x80CA6A8B6C094CC4,
		0x6448050E9C2A7207,
		0x45905BE8654AE067,
		0x703123E5E7D429C2,
		0x132F52BBA570FE92,
		0x93E0DB8440B73A7D,
		0x809549AFC7AEC597,
		0xA0F8A7517A273C05,
		0x16F46FB18C8009E4,
		0xFF6BE494C7987F34,
		0x3D87450E15D98694,
		0x65287525D951F6BE,
		0x2FB897405C90B361,
		0xA4822F1CF23F4810,
		0xDF7E3EEB29642C38,
		0xFFA5D878809819DB,
		0x6C4D0409BA1A2BC2,
		0x8974647ED222EA5F,
	};

	static std::unordered_set<rage::scrNativeHandler> s_OutVectorNativeHandlers{};
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

bool JIT::NeedToFixVecRefrs(rage::scrNativeHandler handler)
{
	if (s_OutVectorNativeHandlers.empty())
	{
		for (auto element : s_OutVectorNatives)
		{
			s_OutVectorNativeHandlers.insert((rage::scrNativeHandler)NewBase::Pointers.m_GetNativeHandler(NewBase::Pointers.m_NativesTable, NewBase::GetTranslatedHash(element)));
		}
	}

	return s_OutVectorNativeHandlers.contains(handler);
}
