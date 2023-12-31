#pragma once
#include "BaseHook.hpp"
#include "memory/Module.hpp"
#include "memory/PointerCalculator.hpp"

namespace NewBase
{
	template<typename T = int*>
	class IATHook : public BaseHook
	{
	private:
		void** m_HookLocation;
		void* m_OriginalFunc;
		void* m_HookFunc;

	public:
		IATHook(const std::string_view name, Module* module, const std::string_view library, const std::string_view import, T detour);
		virtual ~IATHook();

		bool Enable();
		bool Disable();

		T Original() const;
	};

	template<typename T>
	inline IATHook<T>::IATHook(const std::string_view name, Module* module, const std::string_view library, const std::string_view import, T detour) :
	    BaseHook(name)
	{
		m_HookLocation = module->GetImport(library, import);
		m_OriginalFunc = *m_HookLocation;
		m_HookFunc     = (void*)(detour);
	}

	template<typename T>
	inline IATHook<T>::~IATHook()
	{
		Disable();
	}

	template<typename T>
	inline bool IATHook<T>::Enable()
	{
		DWORD old_protect;

		VirtualProtect(m_HookLocation, sizeof(m_HookLocation), PAGE_EXECUTE_READWRITE, &old_protect); // we load before Arxan does that
		*m_HookLocation = m_HookFunc;
		VirtualProtect(m_HookLocation, sizeof(m_HookLocation), old_protect, nullptr); // restore old page protection to avoid tripping Arxan when it finally loads
		return true;
	}

	template<typename T>
	inline bool IATHook<T>::Disable()
	{
		*m_HookLocation = m_OriginalFunc;
		return true;
	}

	template<typename T>
	inline T IATHook<T>::Original() const
	{
		return reinterpret_cast<T>(m_OriginalFunc);
	}
}
