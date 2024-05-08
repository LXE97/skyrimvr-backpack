#pragma once
#include "Relocation.h"
#include "SKSE/Impl/Stubs.h"
#include "main_plugin.h"
#include "xbyak/xbyak.h"

namespace hooks
{
	void PostWandUpdateHook();

	void Install();
}