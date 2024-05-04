#include "main_plugin.h"

#include <chrono>

namespace backpackvr
{
	using namespace helper;

	PapyrusVRAPI* g_papyrusvr;

	// user settings, documented in .ini
	bool g_debug_print = true;

	// settings
	bool g_left_hand_mode = false;

	// state
	bool g_use_firstperson = false;

	// resources
	const RE::FormID g_backpack_player_objref_id = 0x12f8;
	std::string      g_mod_name = "BackpackVR.esp";

	// move backpack object to player hand

	// apply art object to backpack

	// iterate through player inventory and extra data

	// drop item, grab with HIGGS, specify rotation

	void Init()
	{
		g_use_firstperson = GetModuleHandleA("vrik") == NULL;
		SKSE::log::info("VRIK {} found", g_use_firstperson ? "not" : "DLL");

		ReadConfig(g_ini_path);

		menuchecker::begin();
		RegisterVRInputCallback();

		/* HIGGS init */
		if (g_higgsInterface)
		{
			auto higgs_config_path = GetGamePath() / "SKSE/Plugins/higgs_vr.ini";

			std::ifstream higgs_config(higgs_config_path);
			float         palm_x = 0.f;
			float         palm_y = 0.f;
			float         palm_z = 0.f;

			if (higgs_config.is_open())
			{
				palm_x = ReadFloatFromIni(higgs_config, "PalmPositionX");
				palm_y = ReadFloatFromIni(higgs_config, "PalmPositionY");
				palm_z = ReadFloatFromIni(higgs_config, "PalmPositionZ");
				higgs_config.close();
			}

			/*
			vrinput::OverlapSphereManager::GetSingleton()->SetPalmOffset(
				{ palm_x, palm_y, palm_z });
			SKSE::log::trace("Palm position set to {} {} {} ", palm_x, palm_y, palm_z);
			*/

			g_higgsInterface->AddPostVrikPostHiggsCallback(OnUpdate);
		}
	}

	void OnGameLoad()
	{
		_DEBUGLOG("Load Game: reset state");

		if (auto f = GetForm(0x007, g_mod_name))
		{
			SKSE::log::trace("fuck {}", f->GetFormEditorID());
		}
	}

	bool OnButtonEvent(const vrinput::ModInputEvent& e)
	{
		if (e.button_ID) {}

		return false;
	}

	void OnUpdate() {}

	bool IsOverlapping(float a_radius_squared)
	{
		if (auto pcvr = RE::PlayerCharacter::GetSingleton()->GetVRNodeData())
		{
			// compute overlap
			auto bow_node = pcvr->ArrowSnapNode;
			auto arrow_node = g_left_hand_mode ? pcvr->LeftWandNode : pcvr->RightWandNode;

			return (arrow_node->world.translate - bow_node->world.translate).SqrLength() <
				a_radius_squared;
		}
		return false;
	}

	void RegisterVRInputCallback()
	{
		auto OVRHookManager = g_papyrusvr->GetOpenVRHook();
		if (OVRHookManager && OVRHookManager->IsInitialized())
		{
			OVRHookManager = RequestOpenVRHookManagerObject();
			if (OVRHookManager)
			{
				SKSE::log::info("Successfully requested OpenVRHookManagerAPI.");

				vrinput::InitControllerHooks();

				vrinput::g_leftcontroller =
					OVRHookManager->GetVRSystem()->GetTrackedDeviceIndexForControllerRole(
						vr::TrackedControllerRole_LeftHand);
				vrinput::g_rightcontroller =
					OVRHookManager->GetVRSystem()->GetTrackedDeviceIndexForControllerRole(
						vr::TrackedControllerRole_RightHand);

				vrinput::g_IVRSystem = OVRHookManager->GetVRSystem();

				OVRHookManager->RegisterControllerStateCB(vrinput::ControllerInputCallback);
				OVRHookManager->RegisterGetPosesCB(vrinput::ControllerPoseCallback);
			}
		}
		else { SKSE::log::trace("Failed to initialize OVRHookManager"); }
	}

	bool ReadConfig(const char* a_ini_path)
	{
		using namespace std::filesystem;
		static std::filesystem::file_time_type last_read = {};

		auto config_path = helper::GetGamePath() / a_ini_path;

		if (auto setting = RE::GetINISetting("bLeftHandedMode:VRInput"))
		{
			g_left_hand_mode = setting->GetBool();
		}

		SKSE::log::info("bLeftHandedMode: {}\n ", g_left_hand_mode);

		try
		{
			auto last_write = last_write_time(config_path);

			if (last_write > last_read)
			{
				std::ifstream config(config_path);
				if (config.is_open())
				{
					g_debug_print = helper::ReadIntFromIni(config, "bDebug");

					config.close();
					last_read = last_write_time(config_path);
					return true;
				}
				else
				{
					SKSE::log::error("error opening ini");
					last_read = file_time_type{};
				}
			}
			else { _DEBUGLOG("ini not read (no changes)"); }
		} catch (const filesystem_error&)
		{
			SKSE::log::error("ini not found, using defaults");
			last_read = file_time_type{};
		}
		return false;
	}

}