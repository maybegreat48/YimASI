#include "Pointers.hpp"

#include "memory/BytePatch.hpp"
#include "memory/ModuleMgr.hpp"
#include "memory/PatternScanner.hpp"
#include "util/Joaat.hpp"

namespace NewBase
{
	bool Pointers::Init()
	{
		auto scanner = PatternScanner(ModuleMgr::Get("GTA5.exe"_J));

		strcpy(ModuleMgr::Get("GTA5.exe"_J)->GetPdbFilePath(), (std::filesystem::current_path() / "GTA5.pdb").string().c_str());

		if (!scanner.Scan())
		{
			LOG(FATAL) << "Some patterns could not be found, unloading.";
			return false;
		}

		return true;
	}

	bool Pointers::InitScriptHook()
	{
		ModuleMgr::Refresh();

		auto scanner = PatternScanner(ModuleMgr::Get("ScriptHookV.dll"_J));

		constexpr auto onlineCheck = Pattern<"74 3A 48 8D 0D">("OnlineCheck");
		scanner.Add(onlineCheck, [this](PointerCalculator ptr) {
			BytePatch::Make(ptr.As<void*>(), std::to_array({0xEB}))->Apply();
		});

		constexpr auto poolStuff = Pattern<"41 81 FA 2C 23 82 11">("PoolStuff");
		scanner.Add(poolStuff, [this](PointerCalculator ptr) {
			BytePatch::Make(ptr.As<void*>(), std::vector<uint8_t>(34, 0x90))->Apply();
			BytePatch::Make(ptr.Add(34).As<void*>(), std::to_array({0xEB}))->Apply();
		});

		constexpr auto poolStuff2 = Pattern<"B8 00 01 00 00 3B C3">("PoolStuff2");
		scanner.Add(poolStuff2, [this](PointerCalculator ptr) {
			BytePatch::Make(ptr.As<void*>(), std::vector<uint8_t>(10, 0x90))->Apply();
		});

		scanner.Scan();

		return true;
	}
}