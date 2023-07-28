#pragma once
#include "Module.hpp"
#include "common.hpp"

namespace NewBase
{
	using joaat_t = std::uint32_t;

	class ModuleMgr
	{
	public:
		ModuleMgr()                                = default;
		virtual ~ModuleMgr()                       = default;
		ModuleMgr(const ModuleMgr&)                = delete;
		ModuleMgr(ModuleMgr&&) noexcept            = delete;
		ModuleMgr& operator=(const ModuleMgr&)     = delete;
		ModuleMgr& operator=(ModuleMgr&&) noexcept = delete;


		static Module* Get(const std::string_view name)
		{
			return GetInstance().GetImpl(name);
		}

		static Module* Get(joaat_t hash)
		{
			return GetInstance().GetImpl(hash);
		}

		/**
		 * @brief Loads the modules from PEB and caches them.
		 * 
		 * @return true If the peb is found and modules have been cached.
		 * @return false If the peb or peb->Ldr pointer were invalid. 
		 */
		static bool Init()
		{
			if (GetInstance().m_CachedModules.empty())
				return GetInstance().InitImpl();

			return true;
		};

		/**
		 * @brief Refresh loaded modules from the PEB and caches them.
		 * 
		 * @return true If the peb is found and modules have been cached.
		 * @return false If the peb or peb->Ldr pointer were invalid. 
		 */
		static bool Refresh()
		{
			return GetInstance().InitImpl();
		}

	private:
		std::unordered_map<std::uint32_t, std::unique_ptr<Module>> m_CachedModules;

		static ModuleMgr& GetInstance()
		{
			static ModuleMgr i{};
			return i;
		}

		bool InitImpl();
		Module* GetImpl(const std::string_view name);
		Module* GetImpl(joaat_t hash);
	};
}