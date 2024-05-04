#pragma once
#pragma warning(push)
#pragma warning(disable: 4100)
#pragma warning(disable: 4189)
#pragma warning(disable: 4244)  // double to float
#pragma warning(disable: 4245)  // signed to unsigned
#pragma warning(disable: 4305)
#pragma warning(disable: 5105)

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include <REL/Relocation.h>

using namespace std::literals;

namespace stl
{
	using namespace SKSE::stl;

	template <class T>
	void write_thunk_call(std::uintptr_t a_src)
	{
		SKSE::AllocTrampoline(64);

		auto& trampoline = SKSE::GetTrampoline();
		T::func = trampoline.write_call<5>(a_src, T::thunk);
	}

	template <class F, std::size_t idx, class T>
	void write_vfunc()
	{
		REL::Relocation<std::uintptr_t> vtbl{ F::VTABLE[0] };
		T::func = vtbl.write_vfunc(idx, T::thunk);
	}

	template <std::size_t idx, class T>
	void write_vfunc(REL::VariantID id)
	{
		REL::Relocation<std::uintptr_t> vtbl{ id };
		T::func = vtbl.write_vfunc(idx, T::thunk);
	}
}
