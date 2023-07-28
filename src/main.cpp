#include "AsiLoader.hpp"
#include "Hijack.hpp"
#include "common.hpp"
#include "filemgr/FileMgr.hpp"
#include "hooking/Hooking.hpp"
#include "memory/ModuleMgr.hpp"
#include "pointers/Pointers.hpp"

namespace NewBase
{
	void Main()
	{
		std::filesystem::path base_dir = std::getenv("appdata");
		base_dir /= "YimMenu";
		FileMgr::Init(base_dir);

		LogHelper::Init("", FileMgr::GetProjectFile("./yimasi.log").Path(), false);

		try
		{
			if (Pointers.Init())
			{
				Hooking::Init();
				AsiLoader::Init();
			}
		}
		catch (const std::exception& e)
		{
			LOG(FATAL) << e.what();
		}
	}
}

BOOL WINAPI DllMain(HINSTANCE dllInstance, DWORD reason, void*)
{
	using namespace NewBase;

	DisableThreadLibraryCalls(dllInstance);

	if (reason == DLL_PROCESS_ATTACH)
	{
		ModuleMgr::Init();
		Hijack::Init(&Main);

		g_DllInstance = dllInstance;
	}
	return true;
}