#pragma once
// clang-format off
#include <Windows.h>
#undef max

#include <AsyncLogger/Logger.hpp>
#include <MinHook.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <string_view>
#include <thread>
#include <vector>
using namespace al;

#include "logger/LogHelper.hpp"


namespace NewBase
{
	using namespace std::chrono_literals;

	extern HINSTANCE g_DllInstance;
	
	extern uint32_t g_LastPoolHash;
	extern std::unordered_map<void*, uint32_t> g_Pools;
}

// clang-format on