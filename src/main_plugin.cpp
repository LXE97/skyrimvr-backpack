#include "main_plugin.h"

#include "NiVectorExtraData.h"

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
	RE::TESObjectREFR* g_marker_disable_objref = nullptr;
	RE::NiPoint3       g_rollover_default_hand_pos;
	RE::NiMatrix3      g_rollover_default_hand_rot;

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
	bool debug_grab(const vrinput::ModInputEvent& e);

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

		hooks::Install();

		auto containerSink = EventSink<TESContainerChangedEvent>::GetSingleton();
		ScriptEventSourceHolder::GetSingleton()->AddEventSink(containerSink);
		containerSink->AddCallback(OnContainerChanged);

		/* input callbacks */
		vrinput::AddCallback(debug_PrintInventoryExtraData,
			vr::EVRButtonId::k_EButton_SteamVR_Trigger, vrinput::Hand::kLeft,
			vrinput::ActionType::kPress);

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
				g_marker_disable_objref = ref;
			}
		}

		if (RE::PlayerCharacter::GetSingleton()->Is3DLoaded())
		{
			auto pcvr = RE::PlayerCharacter::GetSingleton()->GetVRNodeData();
			auto hand = pcvr->RightWandNode;
			auto rollover = pcvr->RoomNode->GetObjectByName(g_rollover_nodename);

			g_rollover_default_hand_pos = rollover->local.translate;
			g_rollover_default_hand_rot = rollover->local.rotate;

			_DEBUGLOG("default rollover transform:");
			helper::PrintVec(g_rollover_default_hand_pos);
		}
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
					g_maximum_item_radius =
						helper::ReadFloatFromIni(config, "fMaximumDesiredItemSize");

					//backpack::g_mini_items_per_row = helper::ReadIntFromIni(config, "iItemsPerRow");
					//backpack::g_mini_horizontal_spacing =
					//	helper::ReadFloatFromIni(config, "fHorizontalSpacing");

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

					if (auto extra = data->extraLists)
					{
						for (auto edata : *extra)
						{
							SKSE::log::trace("EXTRA NAME: {} ", edata->GetDisplayName(key));

							for (auto& extraentry : *edata)
							{
								SKSE::log::trace(
									"extra data type: {:x}", (int)extraentry.GetType());
							}
						}
					}

					if (auto model = key->As<TESModelTextureSwap>())
					{
						SKSE::log::trace("{} ({}) {}: type: {}  model: {}", name, count,
							data->GetDisplayName(), FormTypeToString(key->GetFormType()),
							model->GetModel());
					}
					else if (key->GetFormType() == FormType::Armor)
					{
						if (auto model2 = key->As<TESBipedModelForm>())
						{
							SKSE::log::trace("{} ({}) : type: {}  model: {}", name, count,
								FormTypeToString(key->GetFormType()),
								model2->worldModels[0].GetModel());
						}
					}
					else
					{
						SKSE::log::trace("{} ({}) : type: {}  model: {}", name, count,
							FormTypeToString(key->GetFormType()), "none");
					}
				}
			}
		}
		return false;
	}

	bool debug_grab(const vrinput::ModInputEvent& e)
	{
		/*
		if (e.button_state == vrinput::ButtonState::kButtonDown)
		{
			if (!g_backpacks.empty())
			{
				if (!g_backpacks.front().object->Is3DLoaded())
				{
					
					//g_backpacks.front().object->SetActivationBlocked(true);
					g_backpacks.front().object->MoveTo(RE::PlayerCharacter::GetSingleton());
					SetRolloverObject(g_backpacks.front().object, g_backpack_obj);
					ResetRollover();
					DisableRollover(g_backpacks.front().object, true, true);
					g_movepack = true;
				}
				else
				{
					MoveBackpack();
					g_backpacks.front().object->data.angle = RE::NiPoint3();
					g_movepack = true;
				}
			}
		}
		else { g_movepack = false; }
*/
		return false;
	}
}

namespace RE
{
	ExtraEditorRefMoveData::~ExtraEditorRefMoveData() = default;
	ExtraDataType ExtraEditorRefMoveData::GetType() const
	{
		return ExtraDataType::kEditorRefMoveData;
	}

	ExtraCachedScale::~ExtraCachedScale() = default;
	ExtraDataType ExtraCachedScale::GetType() const { return ExtraDataType::kCachedScale; }

}