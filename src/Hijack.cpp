#include "Hijack.hpp"

#include "memory/ModuleMgr.hpp"
#include "util/Joaat.hpp"

FARPROC p_WTSUnRegisterSessionNotification;
FARPROC p_WTSRegisterSessionNotification;
FARPROC p_WTSFreeMemory;
FARPROC p_WTSQuerySessionInformationW;

namespace NewBase
{
	BOOL Hijack::SpinCountHookTarget(void* a1, int a2)
	{
		m_MainFunc();
		m_Hook->Disable();
		return m_Hook->Original()(a1, a2);
	}

	void Hijack::LoadOriginalImports()
	{
		char path[MAX_PATH + 13];
		HMODULE orig_dll;
		GetSystemDirectoryA(path, MAX_PATH); // TODO: do we need this?

		strcat(path, "\\WTSAPI32.dll");
		orig_dll = LoadLibraryA(path);

		if (!orig_dll)
		{
			MessageBoxA(NULL, "Cannot load original WTSAPI32.dll", "Bad", 0); // cannot use the logger here
			__fastfail(1);
		}

		p_WTSUnRegisterSessionNotification = GetProcAddress(orig_dll, "WTSUnRegisterSessionNotification");
		p_WTSRegisterSessionNotification   = GetProcAddress(orig_dll, "WTSRegisterSessionNotification");
		p_WTSFreeMemory                    = GetProcAddress(orig_dll, "WTSFreeMemory");
		p_WTSQuerySessionInformationW      = GetProcAddress(orig_dll, "WTSQuerySessionInformationW");
	}

	void Hijack::Init(std::function<void()> main_func)
	{
		m_MainFunc = main_func;
		m_Hook = std::make_unique<IATHook<decltype(&SpinCountHookTarget)>>("HijackHook", ModuleMgr::Get("GTA5.exe"_J), "KERNEL32.dll", "InitializeCriticalSectionAndSpinCount", &SpinCountHookTarget);
		m_Hook->Enable();
		LoadOriginalImports();
	}
}

typedef enum _WTS_INFO_CLASS
{
} WTS_INFO_CLASS;

bool _WTSUnRegisterSessionNotification(HWND hWnd)
{
	return ((decltype(&_WTSUnRegisterSessionNotification))p_WTSUnRegisterSessionNotification)(hWnd);
};

bool _WTSRegisterSessionNotification(HWND hWnd, DWORD dwFlags)
{
	return ((decltype(&_WTSRegisterSessionNotification))p_WTSRegisterSessionNotification)(hWnd, dwFlags);
};

bool _WTSFreeMemory(PVOID pMemory)
{
	return ((decltype(&_WTSFreeMemory))p_WTSFreeMemory)(pMemory);
};

bool _WTSQuerySessionInformationW(HANDLE hServer, DWORD SessionId, WTS_INFO_CLASS WTSInfoClass, LPWSTR* ppBuffer, DWORD* pBytesReturned)
{
	return ((decltype(&_WTSQuerySessionInformationW))p_WTSQuerySessionInformationW)(hServer, SessionId, WTSInfoClass, ppBuffer, pBytesReturned);
};