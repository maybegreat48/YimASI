#include "common.hpp"

namespace NewBase
{
	HINSTANCE g_DllInstance{nullptr};
	uint32_t g_LastPoolHash{};
	std::unordered_map<void*, uint32_t> g_Pools{};
}