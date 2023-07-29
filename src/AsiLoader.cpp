#include "AsiLoader.hpp"

#include "pointers/Pointers.hpp"

namespace NewBase
{
	void AsiLoader::Init()
	{
		if (LoadLibraryW(L"ScriptHookV.dll"))
			Pointers.InitScriptHook();

		for (auto& entry : std::filesystem::recursive_directory_iterator(std::filesystem::current_path()))
			if (entry.path().extension() == ".asi")
				LoadLibraryW(entry.path().wstring().c_str());
	}
}