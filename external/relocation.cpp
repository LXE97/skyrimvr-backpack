// based on  SKSE64_common at https://skse.silverlock.org/
#include "relocation.h"

#pragma warning(disable: 4073)	// yes this is intentional
#pragma init_seg(lib)

static RelocationManager s_relocMgr;

uintptr_t RelocationManager::s_baseAddr = 0;

RelocationManager::RelocationManager()
{
	// If this parameter is NULL, GetModuleHandle returns a handle to the file used to create the calling process (.exe file).
	// char* cast is to stop compiler error regarding ambiguous overloads i.e.
	// SKSE::WinAPI::GetModuleHandle(const char* a_moduleName) noexcept;
	// SKSE::WinAPI::GetModuleHandle(const wchar_t* a_moduleName) noexcept;

	s_baseAddr = reinterpret_cast<uintptr_t>(GetModuleHandle((char*)NULL));
}