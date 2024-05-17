#pragma once

#include "SKSE/Impl/Stubs.h"
#include "VR/OpenVRUtils.h"
#include "VR/PapyrusVRAPI.h"
#include "VR/VRManagerAPI.h"
#include "art_addon.h"
#include "backpack.h"
#include "higgsinterface001.h"
#include "hooks.h"
#include "menu_checker.h"
#include "mod_event_sink.hpp"
#include "vrinput.h"

#define _DEBUGLOG(...) \
	if (backpackvr::g_debug_print) { SKSE::log::trace(__VA_ARGS__); }

namespace backpackvr
{
	constexpr const char* g_ini_path = "SKSE/Plugins/BackpackVR.ini";

	extern PapyrusVRAPI* g_papyrusvr;
	extern bool          g_left_hand_mode;
	extern bool          g_use_firstperson;
	extern bool          g_debug_print;

	void Init();

	void OnGameLoad();

	void OnUpdate();

	void OnContainerChanged(const RE::TESContainerChangedEvent* event);

	void OnHiggsStashed(bool isLeft, RE::TESForm* stashedForm);

	void OnHiggsDrop(bool isLeft, RE::TESObjectREFR* droppedRefr);

	void OnMenuOpenClose(RE::MenuOpenCloseEvent const* evn);

	void UnregisterButtons();
	void RegisterButtons();

	void RegisterVRInputCallback();

	/* returns: true if config file changed */
	bool ReadConfig(const char* a_ini_path);
}
