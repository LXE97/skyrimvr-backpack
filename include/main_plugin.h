#pragma once

#include "SKSE/Impl/Stubs.h"
#include "VR/OpenVRUtils.h"
#include "VR/PapyrusVRAPI.h"
#include "VR/VRManagerAPI.h"
#include "menu_checker.h"
#include "mod_event_sink.hpp"
#include "vrinput.h"
#include "higgsinterface001.h"

#define _DEBUGLOG(...) \
	if (backpackvr::g_debug_print) { SKSE::log::trace(__VA_ARGS__); }

namespace backpackvr
{
	constexpr const char* g_ini_path = "SKSE/Plugins/MODNAME.ini";

	extern PapyrusVRAPI* g_papyrusvr;
	extern bool          g_left_hand_mode;
	extern bool          g_debug_print;

	void Init();

	void OnGameLoad();

	bool OnButtonEvent(const vrinput::ModInputEvent& e);

	void OnUpdate();

	void UnregisterButtons(bool isLeft);
	void RegisterButtons(bool isLeft);

	void RegisterVRInputCallback();

	/* returns: true if config file changed */
	bool ReadConfig(const char* a_ini_path);
}
