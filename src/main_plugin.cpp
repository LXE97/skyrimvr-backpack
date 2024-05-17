#include "main_plugin.h"

#include "NiVectorExtraData.h"
#include "RE/E/ExtraDataList.h"

#include <chrono>

namespace backpackvr
{
	using namespace helper;
	using namespace art_addon;
	using namespace backpack;

	// user settings, documented in .ini
	bool  g_debug_print = true;
	bool  g_hardcore_mode = true;
	float g_maximum_item_radius = 25.f;

	// settings
	bool g_left_hand_mode = false;
	bool g_use_firstperson = false;

	// state

	// resources
	const RE::FormID g_backpack_player_objref_id = 0xD96;
	const RE::FormID g_backpack_player_objref_id2 = 0x804;
	const RE::FormID g_backpack_NPC_objref_id = 0x803;
	const RE::FormID g_marker_modspace_id = 0x801;
	const RE::FormID g_player = 0x14;

	std::string mod_name = backpack::g_mod_name;

	RE::TESBoundObject* g_backpack_obj;
	PapyrusVRAPI*       g_papyrusvr;

	bool debug_PrintInventoryExtraData(const vrinput::ModInputEvent& e);

	void OnMenuOpenClose(RE::MenuOpenCloseEvent const* evn)
	{
		if (!evn->opening && std::strcmp(evn->menuName.data(), "Journal Menu") == 0)
		{
			ReadConfig(g_ini_path);
		}
	}

	void OnContainerChanged(const RE::TESContainerChangedEvent* event)
	{
		backpack::Controller::GetSingleton()->OnContainerChanged(event);
	}

	void OnHiggsStashed(bool isLeft, TESForm* stashedForm)
	{
		backpack::Controller::GetSingleton()->OnHiggsStashed(isLeft, stashedForm);
	}

	void OnHiggsDrop(bool isLeft, RE::TESObjectREFR* droppedRefr)
	{
		backpack::Controller::GetSingleton()->OnHiggsDrop(isLeft, droppedRefr);
	}

	bool OnGrabButton(const vrinput::ModInputEvent& e)
	{
		if (e.button_state == vrinput::ButtonState::kButtonDown)
		{
			backpack::Controller::GetSingleton()->OnGrabAction((bool)e.device, true);
		}
		else { backpack::Controller::GetSingleton()->OnGrabAction((bool)e.device, false); }
		return false;
	}

	bool OnDebugSummonButton(const vrinput::ModInputEvent& e)
	{
		if (e.button_state == vrinput::ButtonState::kButtonDown)
		{
			backpack::Controller::GetSingleton()->DebugSummonPlayerPack();
		}
		return true;
	}

	void OnUpdate()
	{
		backpack::Controller::GetSingleton()->PostWandUpdate();
		art_addon::ArtAddonManager::GetSingleton()->Update();
	}

	void Init()
	{
		g_use_firstperson = GetModuleHandleA("vrik") == NULL;
		SKSE::log::info("VRIK {} found", g_use_firstperson ? "not" : "DLL");

		ReadConfig(g_ini_path);

		menuchecker::begin();
		RegisterVRInputCallback();

		RegisterButtons();

		hooks::Install();

		auto containerSink = EventSink<TESContainerChangedEvent>::GetSingleton();
		ScriptEventSourceHolder::GetSingleton()->AddEventSink(containerSink);
		containerSink->AddCallback(OnContainerChanged);

		auto menu_sink = EventSink<RE::MenuOpenCloseEvent>::GetSingleton();
		menu_sink->AddCallback(OnMenuOpenClose);
		RE::UI::GetSingleton()->AddEventSink(menu_sink);

		/* input callbacks */

		vrinput::AddCallback(debug_PrintInventoryExtraData,
			vr::EVRButtonId::k_EButton_A, vrinput::Hand::kRight,
			vrinput::ActionType::kPress);

		vrinput::AddCallback(OnDebugSummonButton, vr::EVRButtonId::k_EButton_Knuckles_B,
			vrinput::Hand::kRight, vrinput::ActionType::kPress);

		/* HIGGS init */
		if (g_higgsInterface)
		{
			auto higgs_config_path = GetGamePath() / "SKSE/Plugins/higgs_vr.ini";

			std::ifstream higgs_config(higgs_config_path);

			if (higgs_config.is_open())
			{
				float palm_x = ReadFloatFromIni(higgs_config, "PalmPositionX");
				float palm_y = ReadFloatFromIni(higgs_config, "PalmPositionY");
				float palm_z = ReadFloatFromIni(higgs_config, "PalmPositionZ");
				vrinput::g_palm_offset = { palm_x, palm_y, palm_z };
				higgs_config.close();
			}

			//g_higgsInterface->AddPostVrikPostHiggsCallback(OnUpdate);
			g_higgsInterface->AddDroppedCallback(OnHiggsDrop);
			g_higgsInterface->AddStashedCallback(OnHiggsStashed);
		}
	}

	void OnGameLoad()
	{
		// populate references
		if (auto f = GetForm(g_marker_modspace_id, g_mod_name))
		{
			_DEBUGLOG("got marker form");
			if (auto ref = f->AsReference())
			{
				_DEBUGLOG("got marker ref");
				backpack::g_marker_disable_objref = ref;
			}
		}

		if (RE::PlayerCharacter::GetSingleton()->Is3DLoaded())
		{
			auto pcvr = RE::PlayerCharacter::GetSingleton()->GetVRNodeData();
			auto hand = pcvr->RightWandNode;
			auto rollover = pcvr->RoomNode->GetObjectByName(g_rollover_nodename);

			backpack::g_rollover_default_hand_pos = rollover->local.translate;
			backpack::g_rollover_default_hand_rot = rollover->local.rotate;

			_DEBUGLOG("default rollover transform:");
			helper::PrintVec(g_rollover_default_hand_pos);
		}

		backpack::Controller::GetSingleton()->Init();
		backpack::Controller::GetSingleton()->Add(Backpack(g_backpack_player_objref_id, g_player));
	}

	void RegisterButtons()
	{
		vrinput::AddCallback(OnGrabButton, vr::EVRButtonId::k_EButton_SteamVR_Trigger,
			vrinput::Hand::kLeft, vrinput::ActionType::kPress);
		vrinput::AddCallback(OnGrabButton, vr::EVRButtonId::k_EButton_SteamVR_Trigger,
			vrinput::Hand::kRight, vrinput::ActionType::kPress);
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

		if (auto setting = RE::GetINISetting("fActivatePickLength:Interface"))
		{
			backpack::g_default_factivatepicklength = setting->GetFloat();
		}

		SKSE::log::info("fActivatePickLength: {}\n ", backpack::g_default_factivatepicklength);
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
					g_maximum_item_radius =
						helper::ReadFloatFromIni(config, "fMaximumDesiredItemSize");

					auto& settings = backpack::Controller::GetSingleton()->SetSettings();
					settings.allow_equip_swapping =
						helper::ReadIntFromIni(config, "bAllowEquipSwapping");
					settings.disable_grid = helper::ReadIntFromIni(config, "bDisableGrid");
					settings.mini_horizontal_spacing =
						helper::ReadFloatFromIni(config, "fHorizontalSpacing");
					settings.mini_items_per_row = helper::ReadIntFromIni(config, "iItemsPerRow");
					settings.newitems_drop_on_loot = helper::ReadIntFromIni(config, "bDropOnLoot");

					settings.newitems_drop_on_pickup =
						helper::ReadIntFromIni(config, "bDropOnPickup");
					settings.newitems_drop_paused =
						helper::ReadIntFromIni(config, "bDropWhilePaused");
					settings.newitems_drop_to_ground =
						helper::ReadIntFromIni(config, "bDropOnGround");
					settings.shutoff_distance_npc =
						helper::ReadFloatFromIni(config, "fPlayerShutoffDistance");
					settings.shutoff_distance_player =
						helper::ReadFloatFromIni(config, "fNPCShutoffDistance");

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

	bool debug_PrintInventoryExtraData(const vrinput::ModInputEvent& e)
	{
		if (e.button_state == vrinput::ButtonState::kButtonDown)
		{
			auto pc = PlayerCharacter::GetSingleton();
			if (auto cc = pc->GetContainer())
			{
				auto inv = pc->GetInventory();
				for (const auto& entry : inv)
				{
					TESBoundObject* key = entry.first;
					const auto&     value = entry.second;

					int  count = value.first;
					auto data = value.second.get();

					auto name = key->GetName();
					SKSE::log::trace("InventoryEntry {} , count: {}", name, count);

					if (auto extra = data->extraLists)
					{
						for (auto edata : *extra)
						{
							SKSE::log::trace("  extra name: {} count: {}",
								edata->GetDisplayName(key), edata->GetCount());

							for (auto& extraentry : *edata)
							{
								SKSE::log::trace(
									"    extra data type: {:x}", (int)extraentry.GetType());
							}
						}
					}
					else { SKSE::log::trace("  no extralist for {}", name); }
				}
			}
		}
		return false;
	}
}