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
	extern bool          g_debug_print;

	void Init();

	void OnGameLoad();

	bool OnGrabButton(const vrinput::ModInputEvent& e);

	void OnUpdate();

	void OnHiggsDrop(bool isLeft, RE::TESObjectREFR* droppedRefr);

	void OnHiggsGrab(bool isLeft, RE::TESObjectREFR* grabbedRefr);

	bool debug_grab(const vrinput::ModInputEvent& e);

	void ResetRollover();
	void            ClearTransformData(RE::TESObjectREFR* a_target);
	void            WriteTransformData(RE::TESObjectREFR* a_target, RE::NiTransform& a_transform);
	RE::NiTransform WorldToLocal(RE::NiTransform& a_parent, RE::NiTransform& a_child);
	void            MoveBackpack();
	void            DisableRollover(RE::TESObjectREFR* a_target, bool a_disabled);
	void            DisableRollover(RE::TESObjectREFR* a_target, bool a_disabled, bool fake);
	void SetRolloverObject(RE::TESObjectREFR* a_target, RE::TESBoundObject* a_new_base_object);

	void UnregisterButtons(bool isLeft);
	void RegisterButtons(bool isLeft);

	void RegisterVRInputCallback();

	/* returns: true if config file changed */
	bool ReadConfig(const char* a_ini_path);
}
