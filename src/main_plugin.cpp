#include "main_plugin.h"

#include "NiVectorExtraData.h"

#include <chrono>

namespace backpackvr
{
	using namespace helper;
	using namespace art_addon;

	struct BackpackItemView
	{
		enum class Type
		{
			kNatural,
			kMinified
		};

		BackpackItemView(Type type, TESBoundObject* base, int count, ExtraDataList* extradata,
			ArtAddonPtr model) :
			type(type),
			base(base),
			count(count),
			extradata(extradata),
			model(model){};

		Type            type;
		TESBoundObject* base;
		int             count;
		ExtraDataList*  extradata;
		ArtAddonPtr     model;
	};

	struct BackpackEvent
	{
		enum class kEventType
		{
			kNone,
			kDrop,
			kIgnoreNextDropEvent
		};
		virtual kEventType GetType() = 0;
		virtual ~BackpackEvent() = default;
	};

	struct BackpackDropEvent : public BackpackEvent
	{
		BackpackDropEvent(
			RE::TESBoundObject* a_base_object, RE::ExtraDataList* a_extradata, int a_count) :
			to_drop(a_base_object),
			extradata(a_extradata),
			count(a_count){};

		RE::TESBoundObject* to_drop = nullptr;
		RE::ExtraDataList*  extradata = nullptr;
		int                 count = 0;

		kEventType GetType() { return kEventType::kDrop; }
	};

	struct BackpackIgnoreNextDropEvent : public BackpackEvent
	{
		kEventType GetType() { return kEventType::kIgnoreNextDropEvent; }
	};

	struct BackpackAnimation
	{};

	RE::TESBoundObject* g_backpack_obj;
	PapyrusVRAPI*       g_papyrusvr;

	// user settings, documented in .ini
	bool  g_debug_print = true;
	bool  g_hardcore_mode = true;
	float g_maximum_item_radius = 25.f;

	// settings
	bool g_left_hand_mode = false;
	bool g_use_firstperson = false;

	// state
	bool               g_movepack = false;
	RE::TESObjectREFR* g_backpack_player_objref = nullptr;
	RE::TESObjectREFR* g_marker_disable_objref = nullptr;
	bool               g_override_rollover_position = false;
	RE::NiPoint3       g_rollover_default_hand_pos;
	RE::NiMatrix3      g_rollover_default_hand_rot;

	// temp debug state
	std::deque<std::unique_ptr<BackpackEvent>> g_backpack_event_queue;
	std::vector<BackpackItemView>              g_backpack_item_visible;

	// resources
	const RE::FormID g_backpack_player_objref_id = 0xD96;
	const RE::FormID g_marker_modspace_id = 0x801;

	std::string g_mod_name = "BackpackVR.esp";
	std::string g_backpack_grab_nodename = "GrabNode";
	std::string g_backpack_container_nodename = "Box";
	std::string g_backpack_container_extentname = "extent";
	// ty atom
	std::string g_rollover_nodename = "WSActivateRollover";

	// TODO:: this should be a member of Backpack class and read the geometry data from the model
	int   mini_items_per_row = 5;
	float mini_horizontal_spacing = 2;
	float mini_width = 30;
	float mini_height = 40;
	float mini_item_desired_radius = 0;
	float mini_items_per_column = 0;
	float mini_vertical_spacing = 0;

	void ResetRollover();

	void CalculateLayout()
	{
		mini_item_desired_radius =
			(mini_width / mini_items_per_row - mini_horizontal_spacing) * 0.5;

		mini_items_per_column =
			std::floor(mini_height / (2 * mini_item_desired_radius + mini_horizontal_spacing));

		mini_vertical_spacing =
			(mini_height - mini_items_per_column * 2 * mini_item_desired_radius) /
			mini_items_per_column;
	}

	bool debug_grab(const vrinput::ModInputEvent& e)
	{
		if (e.button_state == vrinput::ButtonState::kButtonDown)
		{
			if (g_backpack_player_objref)
			{
				if (!g_backpack_player_objref->Is3DLoaded())
				{
					DisableRollover(g_backpack_player_objref, true, true);
					g_backpack_player_objref->SetActivationBlocked(true);
					g_backpack_player_objref->MoveTo(RE::PlayerCharacter::GetSingleton());
					SetRolloverObject(g_backpack_player_objref, g_backpack_obj);
					ResetRollover();
					g_movepack = true;
				}
				else
				{
					MoveBackpack();
					g_backpack_player_objref->data.angle = RE::NiPoint3();
					g_movepack = true;
				}
			}
		}
		else { g_movepack = false; }

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

	void ClearTransformData(RE::TESObjectREFR* a_target)
	{
		if (a_target)
		{
			if (a_target->extraList.HasType(ExtraDataType::kEditorRefMoveData))
			{
				a_target->extraList.RemoveByType(ExtraDataType::kEditorRefMoveData);
			}
		}
	}

	void WriteTransformData(RE::TESObjectREFR* a_target, RE::NiTransform& a_transform)
	{
		RE::ExtraEditorRefMoveData* temp = new RE::ExtraEditorRefMoveData();
		a_transform.rotate.ToEulerAnglesXYZ(temp->realAngle);
		temp->realLocation = a_transform.translate;
		a_target->extraList.Add(temp);
	}

	NiTransform WorldToLocal(NiTransform& a_parent, NiTransform& a_child)
	{
		NiTransform result;
		result.translate = a_parent.rotate * (a_child.translate - a_parent.translate);
		result.rotate = a_parent.rotate.Transpose() * a_child.rotate;
		return result;
	}

	void MoveBackpack()
	{
		if (g_backpack_player_objref)
		{
			if (auto backpacknode = g_backpack_player_objref->GetCurrent3D())
			{
				if (auto grabnode = backpacknode->GetObjectByName(g_backpack_grab_nodename))
				{
					RE::NiUpdateData ctx;

					auto handpos = RE::PlayerCharacter::GetSingleton()
									   ->GetVRNodeData()
									   ->LeftWandNode.get()
									   ->world.translate;

					backpacknode->local.translate = handpos;

					FaceCamera(backpacknode);

					auto grabvector = backpacknode->local.rotate * grabnode->local.translate;

					backpacknode->local.translate -= grabvector;

					backpacknode->Update(ctx);
				}
			}
		}
	}

	void DisableRollover(RE::TESObjectREFR* a_target, bool a_disabled)
	{
		constexpr RE::COL_LAYER active_layer = RE::COL_LAYER::kNonCollidable;
		constexpr RE::COL_LAYER inactive_layer = RE::COL_LAYER::kPortal;
		if (a_target)
		{
			std::function<void(RE::NiAVObject*)> SetCollision = [&](RE::NiAVObject* a_obj) -> void {
				if (a_obj)
				{
					if (a_obj->collisionObject)
					{
						auto test = a_obj->collisionObject->AsBhkNiCollisionObject();
						auto shape = (RE::bhkShape*)test->body.get();
						a_obj->SetCollisionLayer(a_disabled ? inactive_layer : active_layer);
					}
					else if (auto node = a_obj->AsNode())
					{
						for (auto child : node->GetChildren()) { SetCollision(child.get()); }
					}
				}
			};

			SetCollision(a_target->GetCurrent3D());
		}
	}

	void DisableRollover(RE::TESObjectREFR* a_target, bool a_disabled, bool fake)
	{
		constexpr const char* collision_node_name = "CollisionNode";
		constexpr int         active_layer = 0;
		constexpr int         inactive_layer = 600;
		/* this is a total hack job but I can't figure out how to change collision layer on phantom shapes
		* or move rigidbodies without using MoveTo() */
		if (a_target)
		{
			if (auto root = a_target->GetCurrent3D())
			{
				if (auto a_obj = root->GetObjectByName(collision_node_name))
				{
					if (a_disabled && a_obj->local.translate.z < inactive_layer / 2)
					{
						a_obj->local.translate.z = inactive_layer;
					}
					else if (!a_disabled && a_obj->local.translate.z > inactive_layer / 2)
					{
						a_obj->local.translate.z = active_layer;
					}
				}
			}
		}
	}

	void SetRolloverObject(RE::TESObjectREFR* a_target, RE::TESBoundObject* a_new_base_object)
	{
		if (a_target && a_new_base_object) { a_target->SetObjectReference(a_new_base_object); }
	}

	void SetRolloverHand(RE::TESObjectREFR* a_target_collider, bool isLeft)
	{
		constexpr const char* collision_node_name = "CollisionNode";
		if (a_target_collider)
		{
			if (auto root = a_target_collider->GetCurrent3D())
			{
				if (auto a_obj = root->GetObjectByName(collision_node_name))
				{
					auto handloc = PlayerCharacter::GetSingleton()
									   ->GetVRNodeData()
									   ->RightWandNode->world.translate;
					a_obj->local.translate = a_obj->parent->world.rotate.Transpose() *
						(handloc - a_obj->parent->world.translate);
				}
			}
		}

		if (isLeft) { g_override_rollover_position = true; }
		else { ResetRollover(); }
	}

	void ResetRollover()
	{
		g_override_rollover_position = false;
		if (auto rollover_node =
				PlayerCharacter::GetSingleton()->GetVRNodeData()->RoomNode->GetObjectByName(
					g_rollover_nodename))
		{
			rollover_node->local.translate = g_rollover_default_hand_pos;
			rollover_node->local.rotate = g_rollover_default_hand_rot;

			NiUpdateData ctx;
			rollover_node->Update(ctx);
		}
	}

	void QueueDrop(RE::TESBoundObject* a_base_object, RE::ExtraDataList* a_extradata, int a_count)
	{
		g_backpack_event_queue.push_back(
			std::make_unique<BackpackDropEvent>(a_base_object, a_extradata, a_count));
	}

	void TryDropItemToHiggsOrGround(RE::TESBoundObject* a_base_object,
		RE::ExtraDataList* a_extradata, int a_count, bool a_hardcore)
	{
		auto pc = PlayerCharacter::GetSingleton();
		// check if player has a free hand
		NiAVObject* hand = nullptr;
		bool        isLeft = false;
		if (g_higgsInterface->CanGrabObject(false))
		{
			hand = pc->GetVRNodeData()->RightWandNode.get();
		}
		else if (g_higgsInterface->CanGrabObject(true))
		{
			hand = pc->GetVRNodeData()->LeftWandNode.get();
			isLeft = true;
		}
		if (hand)
		{
			// TODO: do npcs react to this as if it was really dropped?
			if (auto dropref = RE::PlayerCharacter::GetSingleton()->RemoveItem(a_base_object,
					a_count, RE::ITEM_REMOVE_REASON::kDropping, a_extradata, nullptr,
					&(hand->world.translate)))
			{
				g_higgsInterface->GrabObject(dropref.get().get(), false);
			}
		}
		else if (a_hardcore)
		{
			// TODO: look for a nice spot to put it
			RE::PlayerCharacter::GetSingleton()->RemoveItem(
				a_base_object, a_count, RE::ITEM_REMOVE_REASON::kDropping, a_extradata, nullptr);
		}
	}

	/* This event fires before the item is added to inventory */
	void OnHiggsStashed(bool isLeft, TESForm* stashedForm)
	{
		g_backpack_event_queue.push_back(std::make_unique<BackpackIgnoreNextDropEvent>());
	}

	void OnContainerChanged(const RE::TESContainerChangedEvent* event)
	{
		auto pc = PlayerCharacter::GetSingleton();
		if (event->oldContainer == pc->GetFormID())
		{  // remove our extradata from all items that left player inventory
			// TODO: for other NPCS as well
			if (auto formID = event->reference.native_handle())
			{
				if (auto form = RE::TESForm::LookupByID(formID))
				{
					if (auto ref = form->AsReference()) { ClearTransformData(ref); }
				}
			}
		}
		else if (event->newContainer == pc->GetFormID())
		{  // item added to player
			if (auto form = RE::TESForm::LookupByID(event->baseObj))
			{
				if (auto obj = form->As<RE::TESBoundObject>())
				{
					if (pc->GetInventory().contains(obj))
					{
						// now we need to look at every item in the player's inventory that matches
						// this form, and check if it has our extradata
						auto               count = pc->GetInventory()[obj].first;
						RE::ExtraDataList* target_list = nullptr;
						if (auto extra_lists = pc->GetInventory()[obj].second->extraLists)
						{
							for (auto list : *extra_lists)
							{
								if (!list->HasType(RE::ExtraDataType::kEditorRef3DData))
								{
									QueueDrop(obj, list, event->itemCount);
								}
							}
						}
					}
				}
			}
		}
	}

	int GetObjectReferenceCount();

	void AddObjectRefToInventory(TESObjectREFR* a_held_object, TESObjectREFR* a_new_owner)
	{
		if (a_held_object && a_new_owner && a_new_owner->As<Actor>())
		{
			if (a_held_object->IsBook())
			{  // TODO: make this work :(
				a_new_owner->As<Actor>()->PickUpObject(
					a_held_object, a_held_object->extraList.GetCount());
			}
			else
			{
				a_held_object->ActivateRef(
					a_new_owner, 0, 0, a_held_object->extraList.GetCount(), false);
			}
		}
	}

	void OnHiggsDrop(bool isLeft, TESObjectREFR* droppedRefr)
	{
		// TODO: check if backpack is active
		if (g_backpack_player_objref && droppedRefr)
		{
			if (auto backpack_node = g_backpack_player_objref->GetCurrent3D()->GetObjectByName(
					g_backpack_container_nodename))
			{
				if (auto box_size = backpack_node->GetExtraData<NiVectorExtraData>(
						g_backpack_container_extentname))
				{
					NiPoint3 temp = { box_size->m_vector[0], box_size->m_vector[1],
						box_size->m_vector[2] };

					NiPoint3 location;
					if (auto drop_geo = droppedRefr->GetCurrent3D()->GetFirstGeometryOfShaderType(
							BSShaderMaterial::Feature::kEnvironmentMap))
					{
						auto test_loc = droppedRefr->data.location;
						location = drop_geo->worldBound.center;
					}
					else
					{
						location = isLeft ? PlayerCharacter::GetSingleton()
												->GetVRNodeData()
												->LeftWandNode->world.translate :
											PlayerCharacter::GetSingleton()
												->GetVRNodeData()
												->RightWandNode->world.translate;
					}

					if (TestBoxCollision(backpack_node->world, temp, location))
					{
						// get object transform relative to backpack space
						auto new_local =
							WorldToLocal(backpack_node->world, droppedRefr->GetCurrent3D()->world);

						// write to extra data
						ClearTransformData(droppedRefr);
						WriteTransformData(droppedRefr, new_local);
						g_backpack_event_queue.push_back(
							std::make_unique<BackpackIgnoreNextDropEvent>());

						// apply model to backpack
						if (auto modelstr = GetObjectModelPath(droppedRefr))
						{
							g_backpack_item_visible.push_back(BackpackItemView(
								BackpackItemView::Type::kNatural, droppedRefr->GetObjectReference(),
								droppedRefr->extraList.GetCount(), &(droppedRefr->extraList),
								ArtAddon::Make(modelstr, g_backpack_player_objref, backpack_node,
									new_local, [](ArtAddon* a) {
										if (auto model = a->Get3D())
										{
											if (model->worldBound.radius > g_maximum_item_radius)
											{
												// scale the model
												model->local.scale = g_maximum_item_radius /
													model->worldBound.radius;
											}
											model->local.scale = 0.5;
										}
									})));
						}

						// add to player inventory
						AddObjectRefToInventory(droppedRefr, RE::PlayerCharacter::GetSingleton());
					}
				}
			}
		}
	}

	void ProcessBackpackEvents()
	{
		// TODO: put in player backpack state
		static bool ignore_next_drop = false;
		while (!g_backpack_event_queue.empty())
		{
			switch (g_backpack_event_queue.front()->GetType())
			{
			case BackpackEvent::kEventType::kDrop:
				{
					if (ignore_next_drop) { ignore_next_drop = false; }
					else
					{
						auto drop_event = dynamic_cast<BackpackDropEvent*>(
							(g_backpack_event_queue.front().get()));

						TryDropItemToHiggsOrGround(drop_event->to_drop, drop_event->extradata,
							drop_event->count, g_hardcore_mode);
					}
				}
			case BackpackEvent::kEventType::kIgnoreNextDropEvent:
				{
					ignore_next_drop = true;
				}
			}
			g_backpack_event_queue.pop_front();
		}
	}

	void QueueAnimation() {}

	void ProcessAnimations() {}

	BackpackItemView* g_hovered_item = nullptr;

	void BackpackUpdate()
	{
		constexpr float reset_distance = 300;
		constexpr float interaction_distance = 80;
		auto            pc = PlayerCharacter::GetSingleton();

		if (g_override_rollover_position)
		{
			if (auto rollover_node =
					pc->GetVRNodeData()->RoomNode->GetObjectByName(g_rollover_nodename))
			{
				auto hand = pc->GetVRNodeData()->LeftWandNode;

				auto desired_world = hand->world.rotate *
					(hand->world.translate +
						NiPoint3(
							0.f, 0.f, 10.f));  //hand->world.rotate * g_rollover_default_hand_pos;

				rollover_node->local.translate = rollover_node->parent->world.rotate.Transpose() *
					(desired_world - rollover_node->world.translate);

				//rollover_node->local.rotate = rollover_node->parent->world.rotate.Transpose()* hand->world.rotate * g_rollover_default_hand_rot;

				NiUpdateData ctx;
				rollover_node->Update(ctx);
			}
		}

		if (g_backpack_player_objref && g_backpack_player_objref->Is3DLoaded())
		{
			auto dist = pc->Get3D(g_use_firstperson)
							->world.translate.GetDistance(
								g_backpack_player_objref->GetCurrent3D()->world.translate);
			// turn off when far away
			if (dist > reset_distance ||
				g_backpack_player_objref->GetParentCell() != pc->GetParentCell())
			{
				g_backpack_item_visible.clear();
				g_backpack_player_objref->MoveTo(g_marker_disable_objref);
				g_override_rollover_position = false;
				ResetRollover();
				SetRolloverObject(g_backpack_player_objref, g_backpack_obj);
			}
			else if (dist < interaction_distance)
			{  // TODO: smarter interaction logic, try use Wearables for everything
				// TODO: on that note, add check to overlap spheres for distance to attached object reference

				auto lhand = pc->GetVRNodeData()->LeftWandNode;
				auto rhand = pc->GetVRNodeData()->RightWandNode;

				// find overlapping object
				for (auto& item : g_backpack_item_visible)
				{
					if (auto node = item.model->Get3D())
					{
						if (item.type == BackpackItemView::Type::kNatural)
						{
							auto pos = node->worldBound.center;

							if (bool isLeft = node->worldBound.radius >
									lhand->world.translate.GetDistance(pos);
								isLeft ||
								node->worldBound.radius > rhand->world.translate.GetDistance(pos))
							{
								if (g_hovered_item != &item)
								{
									g_hovered_item = &item;
									QueueAnimation();
									DisableRollover(g_backpack_player_objref, false, true);
									SetRolloverObject(
										g_backpack_player_objref, g_hovered_item->base);
									SetRolloverHand(g_backpack_player_objref, isLeft);
								}
							}
							else if (g_hovered_item == &item)
							{
								g_hovered_item = nullptr;
								ResetRollover();
								DisableRollover(g_backpack_player_objref, true, true);
								SetRolloverObject(g_backpack_player_objref, g_backpack_obj);
								g_override_rollover_position = false;
							}
						}
						else if (item.type == BackpackItemView::Type::kMinified)
						{
							if (lhand->world.translate.GetDistance(node->world.translate) ||
								rhand->world.translate.GetDistance(node->world.translate))
							{
								if (g_hovered_item != &item)
								{
									g_hovered_item = &item;
									QueueAnimation();
									DisableRollover(g_backpack_player_objref, false, true);
									SetRolloverObject(
										g_backpack_player_objref, g_hovered_item->base);
								}
							}
							else if (g_hovered_item == &item)
							{
								g_hovered_item = nullptr;
								DisableRollover(g_backpack_player_objref, true, true);
							}
						}
					}
				}
			}
		}
	}

	void DebugPrint()
	{
		auto pcvr = PlayerCharacter::GetSingleton()->GetVRNodeData();

		if (auto hand = pcvr->RightWandNode)
		{
			if (auto rolly = pcvr->RoomNode->GetObjectByName(g_rollover_nodename))
			{
				auto hand_to_rollover_vector = rolly->world.translate - hand->world.translate;
				auto hand_to_rollover_rot = rolly->world.rotate.Transpose() * hand->world.rotate;

				NiPoint3 hand_to_rollover_rot_euler;
				hand_to_rollover_rot.ToEulerAnglesXYZ(hand_to_rollover_rot_euler);

				helper::PrintVec(hand_to_rollover_vector);
				helper::PrintVec(hand_to_rollover_rot_euler);
			}
		}
	}

	bool debug_print = false;

	void OnUpdate()
	{
		//TODO put backpack update in manager class
		if (g_movepack) { MoveBackpack(); }
		NiUpdateData ctx;
		ProcessBackpackEvents();

		static int frame_count = 0;
		if (frame_count++ % 180 == 0 && debug_print)
		{
			SKSE::log::trace("print hand to rollover:    ");
			DebugPrint();
		}

		BackpackUpdate();
		if (g_backpack_player_objref && g_backpack_player_objref->GetCurrent3D())
		{
			g_backpack_player_objref->GetCurrent3D()->Update(ctx);
		}

		ArtAddonManager::GetSingleton()->Update();
	}

	bool debug_Button_A(const vrinput::ModInputEvent& e)
	{
		debug_print ^= 1;
		return false;
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
		vrinput::AddCallback(debug_grab, vr::EVRButtonId::k_EButton_SteamVR_Trigger,
			vrinput::Hand::kLeft, vrinput::ActionType::kPress);

		vrinput::AddCallback(debug_Button_A, vr::EVRButtonId::k_EButton_A,
			vrinput::Hand::kRight, vrinput::ActionType::kPress);

		//vrinput::AddCallback(debug_ApplyArt, vr::EVRButtonId::k_EButton_Knuckles_B,
		//	vrinput::Hand::kRight, vrinput::ActionType::kPress);

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
		_DEBUGLOG("Load Game: reset state");
		g_backpack_item_visible.clear();

		// populate references
		if (auto f = GetForm(g_backpack_player_objref_id, g_mod_name))
		{
			_DEBUGLOG("got backpack form");
			if (auto ref = f->AsReference())
			{
				_DEBUGLOG("got backpack ref");
				g_backpack_player_objref = ref;
				g_backpack_obj = ref->GetObjectReference();
			}
		}
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
			_DEBUGLOG("loading player 3d defaults");
			auto pcvr = RE::PlayerCharacter::GetSingleton()->GetVRNodeData();
			auto hand = pcvr->RightWandNode;
			auto rollover = pcvr->RoomNode->GetObjectByName(g_rollover_nodename);

			g_rollover_default_hand_pos = rollover->local.translate;
			g_rollover_default_hand_rot = rollover->local.rotate;
		}
	}

	bool CheckBackpackOverlap()
	{
		if (g_backpack_player_objref)
		{
			if (auto backpacknode = g_backpack_player_objref->GetCurrent3D())
			{
				if (auto boxnode = backpacknode->GetObjectByName(g_backpack_container_nodename))
				{
					if (auto hand = RE::PlayerCharacter::GetSingleton()
										->GetVRNodeData()
										->LeftWandNode.get())
					{
						if (auto box_size = boxnode->GetExtraData<NiVectorExtraData>(
								g_backpack_container_extentname))
						{
							NiPoint3 temp = { box_size->m_vector[0], box_size->m_vector[1],
								box_size->m_vector[2] };

							if (TestBoxCollision(boxnode->world, temp, hand->world.translate))
							{
								return true;
							}
						}
					}
				}
			}
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
					g_maximum_item_radius =
						helper::ReadFloatFromIni(config, "fMaximumDesiredItemSize");
					mini_items_per_row = helper::ReadIntFromIni(config, "iItemsPerRow");
					mini_horizontal_spacing =
						helper::ReadFloatFromIni(config, "fHorizontalSpacing");
					mini_width = helper::ReadFloatFromIni(config, "fWidth");
					mini_height = helper::ReadFloatFromIni(config, "fHeight");

					config.close();
					last_read = last_write_time(config_path);

					CalculateLayout();
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

namespace RE
{
	ExtraEditorRefMoveData::~ExtraEditorRefMoveData() = default;
	ExtraDataType ExtraEditorRefMoveData::GetType() const
	{
		return ExtraDataType::kEditorRefMoveData;
	}
}