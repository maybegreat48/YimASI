#pragma once
#include <script/scrNativeHandler.hpp>

namespace NewBase
{
	constexpr auto NATIVE_COUNT = 6568;
	using crossmap              = std::array<std::pair<rage::scrNativeHash, rage::scrNativeHash>, NATIVE_COUNT>;
	extern const crossmap g_Crossmap;

	static inline rage::scrNativeHash GetTranslatedHash(const rage::scrNativeHash& untranslated)
	{
		for (const auto& mapping : g_Crossmap)
			if (mapping.first == untranslated)
				return mapping.second;

		throw std::runtime_error("Cannot translate native hash");
	}

	static inline rage::scrNativeHash GetUntranslatedHash(const rage::scrNativeHash& translated)
	{
		for (const auto& mapping : g_Crossmap)
			if (mapping.second == translated)
				return mapping.first;

		throw std::runtime_error("Cannot untranslate native hash");
	}
}
