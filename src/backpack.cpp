#include "backpack.h"

namespace backpack
{
	using namespace RE;

	int   g_mini_items_per_row = 5;
	float g_mini_horizontal_spacing = 2;

	void Controller::PostWandUpdate() {}

	void Controller::OnHiggsDrop(bool isLeft, TESObjectREFR* droppedRefr)
	{
		if (auto backpack = GetActiveBackpack(isLeft))
		{
			if (backpack->IsValid() && droppedRefr) { backpack->AddItem(droppedRefr); }
		}
	}
	
	void OnHiggsDrop(bool isLeft, TESObjectREFR* droppedRefr)
	{/*
		if (auto backpack_node = backpack->GetObjectRefr()->GetCurrent3D()->GetObjectByName(
				g_backpack_container_nodename))
		{
			if (auto box_size =
					backpack_node->GetExtraData<NiVectorExtraData>(g_backpack_container_extentname))
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
						backpack->item_view.push_back(BackpackItemView(
							BackpackItemView::Type::kNatural, droppedRefr->GetObjectReference(),
							droppedRefr->extraList.GetCount(), &(droppedRefr->extraList),
							ArtAddon::Make(modelstr, backpack->GetObjectRefr(), backpack_node,
								new_local, [](ArtAddon* a) {
									if (auto model = a->Get3D())
									{
										if (model->worldBound.radius > g_maximum_item_radius)
										{
											// scale the model
											model->local.scale =
												g_maximum_item_radius / model->worldBound.radius;
										}
										model->local.scale = 0.5;
									}
								})));
					}

					// add to player inventory
					AddObjectRefToInventory(droppedRefr, RE::PlayerCharacter::GetSingleton());
				}
			}
		}*/
	}

	void Controller::OnHiggsStashed(bool isLeft, TESForm* stashedForm)
	{
		_DEBUGLOG("hey")
		event_queue.push_back(std::make_unique<BackpackIgnoreNextDropEvent>());
	}

	void Controller::OnContainerChanged(const RE::TESContainerChangedEvent* event)
	{
		//TODO: Check for NPC equip / unequip backpack armor

		if (event->oldContainer &&
			std::any_of(backpacks.begin(), backpacks.end(),
				[event](const Backpack& obj) { return obj.GetWearerID() == event->oldContainer; }))
		{  // Item was removed from one of our backpack references
		}

		if (event->newContainer)
		{
			auto it = std::find_if(backpacks.begin(), backpacks.end(),
				[event](const Backpack& obj) { return obj.GetWearerID() == event->newContainer; });
			if (it != backpacks.end())
			{  // Item was added to one of our backpacks.

				// TODO: implement item exclusion filter

				// Determine source of item

				if (auto id = GetNewItemID(event->baseObj, event->itemCount))
				{  // item came from Higgs stash or backpack
					if (auto transform = GetNewItemTransform(id))
					{
						if (CheckTransformSource(transform)) {}
					}
				}
				else
				{
					if (event->oldContainer)
					{
						// item came from an NPC or container
					}
				}

				// If added to player, check if we need to throw it out
				if (it->GetWearer() == PlayerCharacter::GetSingleton())
				{
					if (!IgnoreNextContainerChange())
					{
						if (TryDropItemToHandOrGround(event->baseObj,
								RE::ExtraDataList * a_extradata, int a_count, bool a_hardcore))
						{
							return;
						}
					}
				}

				// If the backpack is currently open, and the item has no 3D position data,
				// we need to show the new item UI
				if (it->GetState() != Backpack::State::kDisabled)
				{
					// get extra data
					if (auto form = RE::TESForm::LookupByID(event->baseObj))
					{
						if (auto obj = form->As<RE::TESBoundObject>())
						{
							if (!it->IsIgnoredItem(obj)) {}
						}
					}
				}

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

	bool Controller::IgnoreNextContainerChange()
	{
		if (ignore_next_container_add_event)
		{
			ignore_next_container_add_event = false;
			return true;
		}
		return false;
	}

	void Controller::ProcessEvents()
	{
		static bool ignore_next_drop = false;
		while (!event_queue.empty())
		{
			switch (event_queue.front()->GetType())
			{
			case BackpackEvent::kEventType::kDrop:
				{
					if (ignore_next_drop) { ignore_next_drop = false; }
					else
					{
						auto drop_event =
							dynamic_cast<BackpackDropEvent*>((event_queue.front().get()));

						TryDropItemToHandOrGround(drop_event->to_drop, drop_event->extradata,
							drop_event->count, settings.hardcore_mode);
					}
				}
			case BackpackEvent::kEventType::kIgnoreNextDropEvent:
				{
					ignore_next_drop = true;
				}
			}
			event_queue.pop_front();
		}
	}

	void Controller::SetActivator(
		Backpack* a_activation_target, RE::TESBoundObject* a_to_display_on_rollover, bool isLeft)
	{
		// change the displayed item in the rollover UI
		OverrideRolloverReference(a_activation_target->GetObjectRefr(), a_to_display_on_rollover);

		// move the rollover UI to the left hand if needed
		if (isLeft) { SetRolloverOverride(true); }

		// move the backpack's activation collision to the right hand
		constexpr const char* collision_node_name = "CollisionNode";

		if (auto root = a_activation_target->GetObjectRefr()->GetCurrent3D())
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

	void Controller::DisableActivator(Backpack* a_activation_target)
	{
		constexpr const char* collision_node_name = "CollisionNode";
		constexpr int         active_layer = 0;
		constexpr int         inactive_layer = 600;
		/* this is a total hack job but I can't figure out how to change collision layer on phantom shapes
		* or move rigidbodies without using MoveTo() */
		if (a_activation_target && a_activation_target->GetObjectRefr())
		{
			if (auto root = a_activation_target->GetObjectRefr()->GetCurrent3D())
			{
				if (auto a_obj = root->GetObjectByName(collision_node_name))
				{
					a_obj->local.translate.z = inactive_layer;

					ResetRolloverReference(a_activation_target);
					SetRolloverOverride(false);
				}
			}
		}
	}

	void Controller::ResetRolloverPosition()
	{
		if (auto rollover_node =
				PlayerCharacter::GetSingleton()->GetVRNodeData()->RoomNode->GetObjectByName(
					g_rollover_nodename))
		{
			//rollover_node->local.translate = g_rollover_default_hand_pos;
			//rollover_node->local.rotate = g_rollover_default_hand_rot;

			NiUpdateData ctx;
			rollover_node->Update(ctx);
		}
	}

	void Controller::OverrideRolloverPosition()
	{
		auto pcvr = PlayerCharacter::GetSingleton()->GetVRNodeData();

		if (auto rollover_node = pcvr->RoomNode->GetObjectByName(g_rollover_nodename))
		{ /*
			auto hand = pcvr->LeftWandNode;

			auto desired_world =
				hand->world.translate + hand->world.rotate * g_rollover_default_hand_pos;

			rollover_node->local.translate = rollover_node->parent->world.rotate.Transpose() *
				(desired_world - rollover_node->parent->world.translate);

			//rollover_node->local.rotate = rollover_node->parent->world.rotate.Transpose() *
				hand->world.rotate * g_rollover_default_hand_rot;

			NiUpdateData ctx;
			rollover_node->Update(ctx);*/
		}
	}

	void Controller::ResetRolloverReference(Backpack* a_target)
	{
		if (a_target) { a_target->GetObjectRefr()->SetObjectReference(a_target->GetDefaultBase()); }
	}

	void Controller::OverrideRolloverReference(
		RE::TESObjectREFR* a_target, RE::TESBoundObject* a_new_base_object)
	{
		if (a_target && a_new_base_object) { a_target->SetObjectReference(a_new_base_object); }
	}

	void Controller::SendDropEvent(
		RE::TESBoundObject* a_base_object, RE::ExtraDataList* a_extradata, int a_count)
	{
		event_queue.push_back(
			std::make_unique<BackpackDropEvent>(a_base_object, a_extradata, a_count));
	}

	bool Controller::TryDropItemToHandOrGround(RE::TESBoundObject* a_base_object,
		RE::ExtraDataList* a_extradata, int a_count, bool a_hardcore)
	{
		return false;
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
				return true;
			}
		}
		else if (a_hardcore)
		{
			// TODO: look for a nice spot to put it
			RE::PlayerCharacter::GetSingleton()->RemoveItem(
				a_base_object, a_count, RE::ITEM_REMOVE_REASON::kDropping, a_extradata, nullptr);
			return true;
		}
		return false;
	}

	void Backpack::Init()
	{
		if (auto form = helper::GetForm(ref_id, g_mod_name)) { object = form->AsReference(); }
		if (auto form = helper::GetForm(wearer_ref_id, g_mod_name))
		{
			wearer = form->AsReference();
		}
		if (object)
		{
			base = object->GetObjectReference();
			object->SetActivationBlocked(true);
		}
		views.clear();
	}

	void Backpack::CalculateMiniLayout()
	{
		auto& ml = mini_layout;

		if (object && object->Is3DLoaded())
		{
			if (auto container =
					object->GetCurrent3D()->GetObjectByName(g_backpack_container_nodename))
			{
				if (auto data =
						container->GetExtraData<NiVectorExtraData>(g_backpack_container_extentname))
				{
					ml.width = data->m_vector[1];
					ml.height = data->m_vector[2];
				}
			}
		}
		ml.item_desired_radius =
			(ml.width / g_mini_items_per_row - g_mini_horizontal_spacing) * 0.5;

		ml.items_per_column =
			std::floor(ml.height / (2 * ml.item_desired_radius + g_mini_horizontal_spacing));

		ml.vertical_spacing =
			(ml.height - ml.items_per_column * 2 * ml.item_desired_radius) / ml.items_per_column;
	}

	void MoveBackpack()
	{ /*
		if (g_backpacks.front().object->Is3DLoaded())
		{
			if (auto backpacknode = g_backpacks.front().object->GetCurrent3D())
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
		}*/
	}

	void BackpackUpdate()
	{ /*
		constexpr float reset_distance = 300;
		constexpr float interaction_distance = 80;
		auto            pc = PlayerCharacter::GetSingleton();

		for (auto backpack : g_backpacks)
		{
			if (backpack.object->Is3DLoaded())
			{
				auto dist = pc->Get3D(g_use_firstperson)
								->world.translate.GetDistance(
									backpack.object->GetCurrent3D()->world.translate);
				// turn off when far away
				if (dist > reset_distance ||
					backpack.object->GetParentCell() != pc->GetParentCell())
				{
					backpack.item_view.clear();
					backpack.object->MoveTo(g_marker_disable_objref);
					g_override_rollover_position = false;
					SetRolloverObject(backpack.object, backpack.base);
				}
				else if (dist < interaction_distance)
				{  // TODO: smarter interaction logic, try use Wearables for everything
					// TODO: on that note, add check to overlap spheres for distance to attached object reference

					auto lhand = pc->GetVRNodeData()->LeftWandNode;
					auto rhand = pc->GetVRNodeData()->RightWandNode;

					// find overlapping object
					for (auto& item : backpack.item_view)
					{
						if (auto node = item.model->Get3D())
						{
							if (item.type == BackpackItemView::Type::kNatural)
							{
								auto pos = node->worldBound.center;

								if (bool isLeft = node->worldBound.radius >
										lhand->world.translate.GetDistance(pos);
									isLeft ||
									node->worldBound.radius >
										rhand->world.translate.GetDistance(pos))
								{  // overlap enter event
									if (g_hovered_item != &item)
									{
										g_active_backpack[isLeft] = &backpack;
										g_hovered_item = &item;
										QueueAnimation();
										DisableRollover(backpack.object, false, true);
										SetRolloverObject(backpack.object, g_hovered_item->base);
										SetRolloverHand(backpack.object, isLeft);
									}
								}
								else if (g_hovered_item == &item)
								{  // overlap exit event
									g_hovered_item = nullptr;
									DisableRollover(backpack.object, true, true);
									SetRolloverObject(backpack.object, g_backpack_obj);
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
										DisableRollover(backpack.object, false, true);
										SetRolloverObject(backpack.object, g_hovered_item->base);
									}
								}
								else if (g_hovered_item == &item)
								{
									g_hovered_item = nullptr;
									DisableRollover(backpack.object, true, true);
								}
							}
						}
					}
				}
			}
		}*/
	}

}
