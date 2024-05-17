#include "backpack.h"

namespace backpack
{
	using namespace RE;

	RE::NiPoint3       g_rollover_default_hand_pos = {};
	RE::NiMatrix3      g_rollover_default_hand_rot = {};
	RE::TESObjectREFR* g_marker_disable_objref = nullptr;
	float              g_default_factivatepicklength = 180;

	// random
	const uint16_t g_ID_upper = 51261;
	const uint16_t g_ID_lower = 10807;

	uint16_t SetOrGetID(RE::TESObjectREFR* a_obj);
	void     AddTransformData(RE::ExtraDataList* a_edl, RE::NiTransform& a_transform);

	void Controller::DebugSummonPlayerPack()
	{
		auto bp = std::find_if(backpacks.begin(), backpacks.end(),
			[](const Backpack& obj) { return obj.GetWearerID() == kPlayerForm; });
		if (bp != backpacks.end()) { bp->StateTransition(Backpack::State::kGrabbed); }
	}

	void Controller::OnHiggsDrop(bool isLeft, RE::TESObjectREFR* droppedRefr)
	{
		if (auto backpack = GetSelectedBackpack(isLeft))
		{
			if (backpack->IsValid() && droppedRefr)
			{
				// The item is only added if it passes the filters. The 3D model and Item are added in OnContainerChanged
				backpack->InventoryAddObject(isLeft, droppedRefr);
			}
		}
	}

	void Controller::OnGrabAction(bool isLeft, bool start)
	{
		if (start)
		{
			if (auto bp = GetSelectedBackpack(isLeft))
			{
				if (auto view = bp->GetActiveView(isLeft))
				{
					if (view->IsHandle())
					{
						if (bp->GetWearerID() == kPlayerForm)
						{
							bp->StateTransition(Backpack::State::kGrabbed);
						}
					}
					else if (auto item = view->GetActiveItem(isLeft))
					{
						_DEBUGLOG("Item grabbed from backpack: {}", item->base->GetName());
						if (auto dropref = bp->TryDropItem(isLeft, item))
						{
							g_higgsInterface->GrabObject(dropref, isLeft);
						}
					}
				}
			}
		}
		else
		{  // Stop grabbing
			auto bp = std::find_if(backpacks.begin(), backpacks.end(),
				[](const Backpack& obj) { return obj.GetWearerID() == kPlayerForm; });
			if (bp != backpacks.end() && bp->GetState() == Backpack::State::kGrabbed)
			{
				bp->StateTransition(Backpack::State::kActive);
			}
		}
	}

	void Controller::OnFavoriteAction(bool isLeft)
	{
		if (auto bp = GetSelectedBackpack(isLeft))
		{
			if (auto item = bp->GetActiveItem(isLeft)) { bp->ToggleFavoriteItem(isLeft, item); }
		}
	}

	void Controller::OnEquipAction(bool isLeft)
	{
		if (auto bp = GetSelectedBackpack(isLeft))
		{
			if (auto item = bp->GetActiveItem(isLeft)) { bp->TryEquipItem(isLeft, item); }
		}
	}

	void Controller::OnHiggsStashed(bool isLeft, RE::TESForm* stashedForm)
	{
		if (stashedForm)
		{
			if (auto heldref = g_higgsInterface->GetGrabbedObject(isLeft))
			{
				// Add temporary ID to item for later identification
				auto ID = SetOrGetID(heldref);
				// Push empty transform to the queue
				Controller::GetSingleton()->PushNewTransform(RE::NiTransform(),
					heldref->GetObjectReference()->GetFormID(), heldref->extraList.GetCount(), ID,
					isLeft);
			}
		}
	}

	void Controller::OnContainerChanged(const RE::TESContainerChangedEvent* event)
	{
		// Check for items added to visible backpacks, or player backpack
		auto bp = std::find_if(backpacks.begin(), backpacks.end(),
			[event](const Backpack& obj) { return obj.GetWearerID() == event->newContainer; });
		if (bp != backpacks.end() &&
			(bp->GetState() != Backpack::State::kDisabled || bp->GetWearerID() == kPlayerForm))
		{
			_DEBUGLOG("item added to backpack: formid {:x} count {} uid {}", event->baseObj,
				event->itemCount, event->uniqueID);

			if (auto form = TESForm::LookupByID(event->baseObj))
			{
				if (auto bound_obj = form->As<TESBoundObject>())
				{
					// Determine InventoryEntry
					auto inv = bp->GetWearer()->GetInventory();
					auto it = inv.find(bound_obj);
					if (it != inv.end())
					{
						auto&              entry = it->second.second;
						RE::ExtraDataList* target_extra_list = nullptr;

						// If there are multiple extra lists, find one that matches
						if (auto extra = entry->extraLists)
						{
							for (auto edata : *extra)
							{
								_DEBUGLOG("  extra name: {} count: {}",
									edata->GetDisplayName(bound_obj), edata->GetCount());

								// If we have an ID, use that
								if (event->uniqueID)
								{
									if (auto ID = edata->GetByType<RE::ExtraUniqueID>())
									{
										if (ID->uniqueID == event->uniqueID)
										{
											target_extra_list = edata;
											_DEBUGLOG(
												"matching EDL found based on UID {}", ID->uniqueID);
										}
									}
								}
								// Criteria: not equipped, not favorited, no transform, at least itemCount
								else if (edata->GetCount() >= event->itemCount &&
									!edata->HasType(RE::ExtraDataType::kEditorRefMoveData) &&
									!edata->HasType(RE::ExtraDataType::kHotkey) &&
									!edata->HasType(RE::ExtraDataType::kWorn) &&
									!edata->HasType(RE::ExtraDataType::kWornLeft))
								{
									target_extra_list = edata;
									_DEBUGLOG("matching EDL found");
								}
								else { _DEBUGLOG("EDL found but did not match"); }
							}
						}

						// Determine item source
						auto            source = ItemSource::kLoot;
						RE::NiTransform local;
						bool            isLeft = false;

						if (event->oldContainer == 0)
						{  // Only look for transform data if the item came from the world
							auto new_item_data = std::find_if(pending_items.begin(),
								pending_items.end(), [event](const NewItemEvent& e) {
									return e.baseObj == event->baseObj && e.ID == event->uniqueID;
								});

							if (new_item_data != pending_items.end())
							{
								// Check transform data. if intialized, came from backpack. otherwise came from grid
								if (new_item_data->local.translate == RE::NiPoint3::Zero())
								{
									source = ItemSource::kGrid;
								}
								else
								{
									source = ItemSource::kManual;
									local = new_item_data->local;
								}
								isLeft = new_item_data->isLeft;
								pending_items.erase(new_item_data);
							}
							else { source = ItemSource::kPickup; }
						}
						_DEBUGLOG("  Source: {}", (int)source);

						if (event->newContainer != kPlayerForm && source != ItemSource::kManual)
						{  // If this is an NPC backpack, we don't care about dropping items
							source = ItemSource::kGrid;
						}

						View* destination = nullptr;
						switch (source)
						{
						case ItemSource::kLoot:
							if (settings.newitems_drop_on_loot)
							{
								if (!menuchecker::isGameStopped() || settings.newitems_drop_paused)
								{
									auto count = event->itemCount;
									SKSE::GetTaskInterface()->AddTask(
										[this, bound_obj, target_extra_list, count]() {
											this->DropItemToHandOrGround(bound_obj,
												target_extra_list, count,
												settings.newitems_drop_to_ground);
										});
									break;
								}
							}
						case ItemSource::kPickup:
							if (settings.newitems_drop_on_pickup && source != ItemSource::kLoot)
							{
								auto count = event->itemCount;
								SKSE::GetTaskInterface()->AddTask(
									[this, bound_obj, target_extra_list, count]() {
										this->DropItemToHandOrGround(bound_obj, target_extra_list,
											count, settings.newitems_drop_to_ground);
									});
								break;
							}
						case ItemSource::kGrid:
							if (bp->GetState() != Backpack::State::kDisabled)
							{
								if (!settings.disable_grid)
								{  // Add to grid
									if (auto grid = bp->GetViewByName(GridView::kNodeName))
									{  // Just refresh the grid view
										grid->Filter(
											[](const RE::TESBoundObject* obj) { return true; });

										// Remove our UID
										if (target_extra_list &&
											target_extra_list->HasType<RE::ExtraUniqueID>())
										{
											target_extra_list->RemoveByType(
												RE::ExtraDataType::kUniqueID);
										}
									}
									break;
								}
								else if (destination = bp->GetViewByName(NormalView::kNodeName);
										 destination)
								{  // Auto-add to natural view
									local.translate = ((NormalView*)destination)
														  ->FindNextAvailableSlot(bound_obj);
								}
							}
						case ItemSource::kManual:
							if (bp->GetState() != Backpack::State::kDisabled)
							{
								if (auto view = bp->GetActiveView(isLeft))
								{
									destination = view;
									if (std::strcmp(view->GetName(), Holster::kNodeName) == 0)
									{
										local.translate = RE::NiPoint3::Zero();
										local.rotate.SetEulerAnglesXYZ(helper::deg2rad(90), 0, 0);
									}
								}
								if (auto model = helper::GetObjectModelPath(bound_obj);
									model && destination)
								{
									destination->AddItem(
										Item(bound_obj, event->itemCount, target_extra_list,
											art_addon::ArtAddon::Make(model, bp->GetObjectRefr(),
												destination->GetRoot(), local)));

									// Write transform to extradata
									if (target_extra_list)
									{
										auto temp = new RE::ExtraEditorRefMoveData();
										temp->realLocation = local.translate;
										local.rotate.ToEulerAnglesXYZ(temp->realAngle);
										target_extra_list->Add(temp);

										// Remove our UID
										if (target_extra_list->HasType<RE::ExtraUniqueID>())
										{
											target_extra_list->RemoveByType(
												RE::ExtraDataType::kUniqueID);
										}

										auto temp2 = new RE::ExtraHealth();
										temp2->health = 0.69;
										target_extra_list->Add(temp2);
									}
									else
									{  // If the entry doesn't have any extradata, we have to create it in this hack way
										if (auto changes = bp->GetWearer()->GetInventoryChanges())
										{
											changes->SetFavorite(entry.get(), nullptr);
											event_queue.push_back(
												std::make_unique<CreateExtraDataEvent>(
													entry.get(), event->itemCount, local));
											_DEBUGLOG("Sending ExtraData Event");
										}
									}
								}
							}
						}
					}
				}
			}
		}

		// Check for items removed from visible backpacks
		auto bp_remove = std::find_if(backpacks.begin(), backpacks.end(),
			[event](const Backpack& obj) { return obj.GetWearerID() == event->oldContainer; });
		if (bp_remove != backpacks.end() && bp_remove->GetState() != Backpack::State::kDisabled)
		{
			_DEBUGLOG("item removed from backpack: formid {:x} count {}", event->baseObj,
				event->itemCount);

			// TODO: To determine which Item was removed, we need to find all Items in all Views
			// with matching formID and use process of elimination

			// Find the View it was removed from and delete the corresponding Item
			for (auto& v : bp_remove->GetViews())
			{
				auto& items = v->GetItems();
				auto  it = std::find_if(items.begin(), items.end(),
					 [&](const Item& i) { return i.base->formID == event->baseObj; });

				if (it != items.end())
				{
					_DEBUGLOG("  matching baseobj found: {}", it->base->GetName());
					items.erase(it);
				}
			}
		}
	}

	void Controller::OnEquip(const RE::TESEquipEvent* event)
	{
		// Check for NPC equip / unequip backpack armor
		if (event->baseObject == g_backpack_formID)
		{
			// check if we already have a backpack for this npc
			auto it = std::find_if(backpacks.begin(), backpacks.end(),
				[event](const Backpack& obj) { return obj.GetWearer() == event->actor.get(); });
			if (it != backpacks.end())
			{
				if (event->equipped)
				{
					// do nothing
				}
				else
				{
					// backpack was removed, destroy virtual backpack
					backpacks.erase(it);
				}
			}
			else
			{
				if (event->equipped)
				{
					// TODO: create new backpack for this npc
					// backpacks.push_back();
				}
			}
		}
		// If the item exists in an open backpack, we need to remove it from the View
		else if (event->equipped && 0) {}
		// If the item was unequipped by an actor whose backpack is currently open, add it to the View
		else if (!event->equipped && 0) {}
	}

	void Controller::PostWandUpdate()
	{
		ProcessInput();
		ProcessEvents();
	}

	void Controller::ProcessInput()
	{
		std::vector<Backpack*> process;

		// Broad phase checking of backpacks location
		for (auto& bp : backpacks)
		{
			if (bp.GetState() != Backpack::State::kDisabled)
			{
				if (!bp.IsInit()) { bp.Init(); }

				if (bp.GetState() == Backpack::State::kGrabbed) { bp.MoveGrabbed(); }
				else if (bp.CheckInteractDistance())
				{
					bp.StateTransition(Backpack::State::kActive);
					process.push_back(&bp);
				}
				else if (bp.CheckShutoffDistance()) { bp.StateTransition(Backpack::State::kIdle); }
				else { bp.StateTransition(Backpack::State::kDisabled); }
			}
		}

		// Do processing of hand positions to determine interaction events
		for (auto bp : process)
		{
			for (auto isLeft : { true, false })
			{
				auto state = GetHandState(isLeft);

				switch (state)
				{
				case HandState::kGrabbing:

					if (auto grabref = g_higgsInterface->GetGrabbedObject(isLeft))
					{
						if (bp->PickActiveView(grabref->GetCurrent3D()->worldBound.center, isLeft))
						{
							selected_backpack[isLeft] = bp;
						}
					}

					break;
				case HandState::kWeapon:
				case HandState::kEmpty:
					{
						if (auto view = bp->PickActiveView(
								vrinput::GetHandPosition(isLeft, backpackvr::g_use_firstperson),
								isLeft))
						{
							selected_backpack[isLeft] = bp;

							if (auto item = view->PickActiveItem(
									vrinput::GetHandPosition(isLeft, backpackvr::g_use_firstperson),
									isLeft))
							{}
						}
					}
					break;
				default:
					break;
				}
			}
		}

		// Update rollover UI
		static bool reset_rollover = false;
		if (rollover_override)
		{
			OverrideRolloverPosition();
			reset_rollover = true;
		}
		else
		{
			if (reset_rollover)
			{
				reset_rollover = false;
				ResetRolloverPosition();
			}
		}
	}

	void Controller::ProcessEvents()
	{
		// TODO: temporary - effect on hand to show View overlap
		static art_addon::ArtAddonPtr handfx[2];

		while (!event_queue.empty())
		{
			auto event = event_queue.front().get();
			switch (event->type())
			{
			case UIEvent::Type::ViewHovered:
				{
					auto e = (ViewHoverEvent*)event;
					if (e->new_state == View::State::kActive)
					{
						NiTransform temp;
						handfx[e->isLeft] = art_addon::ArtAddon::Make("HelperSphere.nif",
							RE::PlayerCharacter::GetSingleton(),
							vrinput::GetHandNode(e->isLeft, backpackvr::g_use_firstperson), temp);
					}
					else if (e->new_state == View::State::kIdle)
					{
						// When a View goes out of focus, we need to set all its Items to idle..
						for (auto& i : e->view->GetItems())
						{
							i.SetState(e->isLeft, Item::State::kIdle);
						}
						handfx[e->isLeft].reset();
					}
				}
				break;
			case UIEvent::Type::ItemHovered:
				{
					auto e = (ItemHoverEvent*)event;
					_DEBUGLOG("Item hover event: {}  {} on {}", e->isLeft ? "left" : "right",
						e->new_state == Item::State::kActive      ? "active" :
							e->new_state == Item::State::kHovered ? "hovered" :
																	"idle",
						e->item->base->GetName());

					if (selected_backpack[e->isLeft])
					{
						if (e->old_state == Item::State::kActive)
						{
							// de-activate the rollover if there's no item selected for this hand
							if (!(selected_backpack[e->isLeft]->GetActiveItem(e->isLeft)))
							{
								_DEBUGLOG("    disable activator");
								DisableActivator(selected_backpack[e->isLeft]);
							}
						}

						switch (e->new_state)
						{
						case Item::State::kActive:
							{
								// do selection visuals
								auto            item_model = e->item->model->Get3D();
								RE::NiTransform temp;
								temp.translate = item_model->world.rotate.Transpose() *
									(item_model->worldBound.center - item_model->world.translate);
								e->item->effects.clear();
								e->item->effects.push_back(
									art_addon::ArtAddon::Make("HelperSphere.nif",
										selected_backpack[e->isLeft]->GetObjectRefr(), item_model,
										temp, [](art_addon::ArtAddon* a) {
											if (auto model = a->Get3D())
											{
												model->local.scale = 0.7;
											}
										}));

								SetActivator(
									selected_backpack[e->isLeft], e->item->base, e->isLeft);
							}
							break;
						case Item::State::kHovered:
							{
								// do hover visuals
								auto            item_model = e->item->model->Get3D();
								RE::NiTransform temp;
								temp.translate = item_model->world.rotate.Transpose() *
									(item_model->worldBound.center - item_model->world.translate);
								e->item->effects.clear();
								e->item->effects.push_back(
									art_addon::ArtAddon::Make("HelperSphere.nif",
										selected_backpack[e->isLeft]->GetObjectRefr(), item_model,
										temp, [](art_addon::ArtAddon* a) {
											if (auto model = a->Get3D())
											{
												model->local.scale = 0.3;
											}
										}));
							}
							break;
						case Item::State::kIdle:
							e->item->effects.clear();
							break;
						}
					}
				}
				break;
			case UIEvent::Type::CreateExtraDataEvent:
				{
					auto e = (CreateExtraDataEvent*)event;
					if (e->entry)
					{
						if (auto list_of_lists = e->entry->extraLists)
						{
							if (auto edl = list_of_lists->front())
							{
								if (edl->HasType(RE::ExtraDataType::kHotkey))
								{
									edl->RemoveByType(RE::ExtraDataType::kHotkey);
								}

								AddTransformData(edl, e->local);
							}
						}
					}
				}
				break;
			default:
				break;
			}

			event_queue.pop_front();
		}
	}

	HandState Controller::GetHandState(bool isLeft)
	{
		if (g_higgsInterface->IsHandInGrabbableState(isLeft)) { return HandState::kEmpty; }
		else if (g_higgsInterface->GetGrabbedObject(isLeft))
		{
			// TODO: check here if object is depositable in inventory
			return HandState::kGrabbing;
		}
		else if (settings.allow_equip_swapping) { return HandState::kWeapon; }
		return HandState::kInvalid;
	}

	void Controller::SetActivator(
		Backpack* a_activation_target, RE::TESBoundObject* a_to_display_on_rollover, bool isLeft)
	{
		if (auto objref = a_activation_target->GetObjectRefr())
		{
			// The rollover text doesn't refresh until you move off this object, so
			// disable then re-enable it
			DisableActivator(a_activation_target);

			// change the displayed item in the rollover UI
			OverrideRolloverReference(objref, a_to_display_on_rollover);

			// move the rollover UI to the left hand if needed
			if (isLeft) { SetRolloverUIPositionOverride(true); }

			// move activator to hand
			SKSE::GetTaskInterface()->AddTask(
				[this, objref]() { this->MoveActivator(objref, true); });

			//TODO : set activation distance to very small
		}
	}

	void Controller::DisableActivator(Backpack* a_activation_target)
	{
		if (a_activation_target && a_activation_target->GetObjectRefr())
		{
			if (auto root = a_activation_target->GetObjectRefr()->GetCurrent3D())
			{
				MoveActivator(a_activation_target->GetObjectRefr(), false);
				ResetRolloverReference(a_activation_target);
				SetRolloverUIPositionOverride(false);
			}
		}
		//TODO : set activation distance to normal
	}

	void Controller::DropItemToHandOrGround(RE::TESBoundObject* a_base_object,
		RE::ExtraDataList* a_extradata, int a_count, bool a_allow_ground)
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
		else if (a_allow_ground)
		{
			// TODO: look for a nice spot to put it
			RE::PlayerCharacter::GetSingleton()->RemoveItem(
				a_base_object, a_count, RE::ITEM_REMOVE_REASON::kDropping, a_extradata, nullptr);
		}
	}

	bool Backpack::CheckShutoffDistance()
	{
		if (auto playerroot = RE::PlayerCharacter::GetSingleton()->GetCurrent3D())
		{
			if (auto myroot = object->GetCurrent3D())
			{
				if (wearer_ref_id == kPlayerForm)
				{
					return playerroot->world.translate.GetDistance(myroot->world.translate) <
						Controller::GetSingleton()->GetSettings().shutoff_distance_player;
				}
				else
				{
					return playerroot->world.translate.GetDistance(myroot->world.translate) <
						Controller::GetSingleton()->GetSettings().shutoff_distance_npc;
				}
			}
		}
		return false;
	}

	bool Backpack::CheckInteractDistance()
	{
		if (auto playerroot = RE::PlayerCharacter::GetSingleton()->GetCurrent3D())
		{
			if (auto myroot = object->GetCurrent3D())
			{
				static int c = 1;
				if (c % 200 == 0)
				{
					SKSE::log::trace("root dist: {}",
						playerroot->world.translate.GetDistance(myroot->world.translate));
				}
				return playerroot->world.translate.GetDistance(myroot->world.translate) <
					Controller::GetSingleton()->GetSettings().min_interaction_distance;
			}
		}

		return false;
	}

	Item* View::GetActiveItem(bool isLeft)
	{
		for (auto& i : items)
		{
			if (i.GetState(isLeft) == Item::State::kActive) { return &i; }
		}
		return nullptr;
	}

	View* Backpack::GetActiveView(bool isLeft) const
	{
		for (auto& v : views)
		{
			if (v->GetState(true) == View::State::kActive ||
				v->GetState(false) == View::State::kActive)
			{
				return v.get();
			}
		}
		return nullptr;
	}

	View* Backpack::GetViewByName(std::string a_name) const
	{
		for (auto& v : views)
		{
			if (std::strcmp(v->GetName(), a_name.c_str()) == 0) { return v.get(); }
		}
		return nullptr;
	}

	bool Backpack::InventoryAddObject(bool isLeft, RE::TESObjectREFR* a_target_ref)
	{
		if (a_target_ref)
		{
			if (auto view = GetActiveView(isLeft))
			{
				return view->InventoryAddObject(a_target_ref, wearer, isLeft);
			}
		}
		return false;
	}

	RE::TESObjectREFR* Backpack::TryDropItem(bool isLeft, Item* a_item)
	{
		if (a_item)
		{
			RE::NiPoint3 droploc = a_item->model->Get3D()->world.translate;
			RE::NiPoint3 droprot;
			a_item->model->Get3D()->world.rotate.ToEulerAnglesXYZ(droprot);

			if (auto handle = RE::PlayerCharacter::GetSingleton()->RemoveItem(a_item->base,
					a_item->count, RE::ITEM_REMOVE_REASON::kDropping, a_item->extradata, nullptr,
					&droploc, &droprot))
			{
				if (auto deref = handle.get()) { return deref.get(); }
			}
		}

		return nullptr;
	}

	void Backpack::ToggleFavoriteItem(bool isLeft, Item* a_item) {}

	void Backpack::TryEquipItem(bool isLeft, Item* a_item)
	{
		// If its a non-consumable, we need to check if it will replace an already-equipped item
		if (0)
		{
			if (0)
			{
				// Push the current transform, to be applied to the unequipped item in OnEquipEvent
			}
		}

		// equip/consume the item
	}

	void Backpack::StateTransition(State a_state)
	{
		if (state == a_state) { return; }

		// OnExit State event
		switch (state)
		{
		case State::kDisabled:
			{  // do init
				if (object) { object->MoveTo(RE::PlayerCharacter::GetSingleton()); }

				Controller::GetSingleton()->DisableActivator(this);
				// parse nif for views, populate items
				Init();
			}
			break;
		case State::kIdle:
			break;
		case State::kActive:
			break;
		}

		// OnEnter state event
		switch (a_state)
		{
		case State::kDisabled:
			{  // do shut off cleanup
				views.clear();
				if (g_marker_disable_objref) { object->MoveTo(g_marker_disable_objref); }
			}
			break;
		case State::kIdle:
			break;
		case State::kActive:
			break;
		}

		state = a_state;
	}

	void Backpack::Init()
	{
		if (object->Is3DLoaded())
		{
			if (auto root = object->GetCurrent3D())
			{
				for (auto& c : root->AsNode()->GetChildren())
				{
					if (c->name.contains(NormalView::kNodeName))
					{
						views.push_back(std::make_unique<NormalView>(c.get()));
					}
					else if (c->name.contains(GridView::kNodeName))
					{
						views.push_back(std::make_unique<GridView>(c.get()));
					}
					else if (c->name.contains(Holster::kNodeName))
					{
						views.push_back(std::make_unique<Holster>(c.get()));
					}
					else if (c->name.contains(Handle::kNodeName))
					{
						views.push_back(std::make_unique<Handle>(c.get()));
					}
				}

				auto inv = wearer->GetInventory();
				for (const auto& mapitem : inv)
				{
					const auto& entry = mapitem.second.second;
					int         count = mapitem.second.first;

					if (auto extra = entry->extraLists)
					{
						for (auto extralist : *extra)
						{
							count -= extralist->GetCount();
							for (auto& view : views)
							{
								if (view->AddItemEx(
										entry->object, extralist, extralist->GetCount()))
								{
									_DEBUGLOG("{} added to {} : count = {} (extradata)",
										extralist->GetDisplayName(entry->object), view->GetName(),
										extralist->GetCount());
									break;
								}
							}
						}
					}
					if (count)
					{
						for (auto& view : views)
						{
							if (view->AddItemEx(entry->object, nullptr, count))
							{
								_DEBUGLOG("{} added to {} : count = {}", entry->object->GetName(),
									view->GetName(), count);
								break;
							}
						}
					}
				}
			}
		}
	}

	View* Backpack::PickActiveView(const RE::NiPoint3& a_world_pos, bool isLeft)
	{
		View* selected = nullptr;
		for (auto& v : views)
		{
			if (v->CheckOverlap(a_world_pos) > 0)
			{
				v->SetState(isLeft, View::State::kActive);
				selected = v.get();
			}
			else { v->SetState(isLeft, View::State::kIdle); }
		}
		return selected;
	}

	void View::SetState(bool isLeft, State a_new_state)
	{
		if (state[isLeft] != a_new_state)
		{
			Controller::GetSingleton()->PushUIEvent<ViewHoverEvent>(a_new_state, isLeft, this);

			state[isLeft] = a_new_state;
		}
	}

	float View::CheckOverlap(const RE::NiPoint3& a_world_pos)
	{
		auto local = root->world.rotate * (a_world_pos - root->world.translate);
		if (local.x > min_bound.x && local.x < max_bound.x && local.y > min_bound.y &&
			local.y < max_bound.y && local.z > min_bound.z && local.z < max_bound.z)
		{
			return local.Length();
		}

		return -1.f;
	}

	float Item::CheckOverlap(const RE::NiPoint3& a_world_pos)
	{
		if (model && model->Get3D())
		{
			auto dist = model->Get3D()->worldBound.center.GetDistance(a_world_pos);
			if (dist < model->Get3D()->worldBound.radius) { return dist; }
		}
		return -1.f;
	}

	void Item::SetState(bool isLeft, State a_new_state)
	{
		if (state[isLeft] != a_new_state)
		{
			Controller::GetSingleton()->PushUIEvent<ItemHoverEvent>(
				a_new_state, state[isLeft], isLeft, this);
			state[isLeft] = a_new_state;
		}
	}

	Item* View::PickActiveItem(const RE::NiPoint3& a_world_pos, bool isLeft)
	{
		Item*                                selected = nullptr;
		std::vector<std::pair<float, Item*>> narrow;

		for (auto& i : items)
		{
			// broad phase
			if (float dist = i.CheckOverlap(a_world_pos); dist > 0)
			{
				narrow.push_back(std::make_pair(dist, &i));
			}
			else { i.SetState(isLeft, Item::State::kIdle); }
		}

		// find the closest item, set it to Active, set the rest to Hovered
		if (!narrow.empty())
		{
			std::sort(narrow.begin(), narrow.end(),
				[](const auto& a, const auto& b) { return a.first < b.first; });

			selected = narrow.front().second;
			selected->SetState(isLeft, Item::State::kActive);

			for (size_t i = 1; i < narrow.size(); ++i)
			{
				narrow[i].second->SetState(isLeft, Item::State::kHovered);
			}
		}

		return selected;
	}

	bool View::InventoryAddObject(
		RE::TESObjectREFR* a_obj, RE::TESObjectREFR* a_wearer, bool isLeft)
	{
		if (a_obj && a_wearer)
		{
			if (auto base = a_obj->GetObjectReference(); base && CanAcceptObject(base))
			{
				if (auto node = a_obj->GetCurrent3D())
				{
					// Add ID to extradata for later identification
					auto ID = SetOrGetID(a_obj);

					// Push empty transform to the queue
					Controller::GetSingleton()->PushNewTransform(
						helper::WorldToLocal(root->world, node->world), base->GetFormID(),
						a_obj->extraList.GetCount(), ID, isLeft);
				}
				// Attempt to add the item to inventory
				AddObjectRefToInventory(a_obj, a_wearer);

				return true;
			}
		}
		return false;
	}

	bool GridView::InventoryAddObject(
		RE::TESObjectREFR* a_obj, RE::TESObjectREFR* a_wearer, bool isLeft)
	{
		if (a_obj && a_wearer)
		{
			if (auto base = a_obj->GetObjectReference(); base && CanAcceptObject(base))
			{
				if (auto node = a_obj->GetCurrent3D())
				{
					// Add ID to extradata for later identification
					auto ID = SetOrGetID(a_obj);

					// Push empty transform to the queue
					Controller::GetSingleton()->PushNewTransform(RE::NiTransform(),
						base->GetFormID(), a_obj->extraList.GetCount(), ID, isLeft);
				}
				AddObjectRefToInventory(a_obj, a_wearer);

				return true;
			}
		}
		return false;
	}

	void View::Filter(std::function<bool(const RE::TESBoundObject*)> filter) {}

	void GridView::Filter(std::function<bool(const RE::TESBoundObject*)> filter) {}

	bool View::AddItemEx(RE::TESBoundObject* a_base, RE::ExtraDataList* a_extra, int count)
	{
		if (CanAcceptObject(a_base))
		{
			if (auto model = helper::GetObjectModelPath(a_base); model)
			{
				RE::NiTransform temp;
				if (a_extra)
				{
					if (auto local = a_extra->GetByType<RE::ExtraEditorRefMoveData>())
					{
						temp.translate = local->realLocation;
						temp.rotate.EulerAnglesToAxesZXY(local->realAngle);

						AddItem(Item(a_base, count, a_extra,
							art_addon::ArtAddon::Make(
								model, RE::PlayerCharacter::GetSingleton(), GetRoot(), temp)));
					}
				}
			}
		}
		return false;
	}

	bool Holster::AddItemEx(RE::TESBoundObject* a_base, RE::ExtraDataList* a_extra, int count)
	{
		if (CanAcceptObject(a_base))
		{
			if (auto model = helper::GetObjectModelPath(a_base); model)
			{
				RE::NiTransform temp;
				if (a_extra)
				{
					if (auto local = a_extra->GetByType<RE::ExtraEditorRefMoveData>())
					{
						temp.rotate.EulerAnglesToAxesZXY(helper::deg2rad(-90), 0, 0);

						AddItem(Item(a_base, count, a_extra,
							art_addon::ArtAddon::Make(
								model, RE::PlayerCharacter::GetSingleton(), GetRoot(), temp)));
					}
				}
			}
		}
		return false;
	}

	bool GridView::AddItemEx(RE::TESBoundObject* a_base, RE::ExtraDataList* a_extra, int count)
	{
		return false;
	}

	void GridView::CalculateGridLayout()
	{
		auto& settings = Controller::GetSingleton()->GetSettings();
		auto& ml = mini_layout;

		if (root)
		{
			if (auto data = root->GetExtraData<NiVectorExtraData>(g_backpack_container_extentname))
			{
				ml.width = data->m_vector[1];
				ml.height = data->m_vector[2];
			}
		}

		ml.item_desired_radius =
			(ml.width / settings.mini_items_per_row - settings.mini_horizontal_spacing) * 0.5;

		ml.items_per_column =
			std::floor(ml.height / (2 * ml.item_desired_radius + settings.mini_horizontal_spacing));

		ml.vertical_spacing =
			(ml.height - ml.items_per_column * 2 * ml.item_desired_radius) / ml.items_per_column;
	}

	bool GridView::CanAcceptObject(RE::TESBoundObject* a_obj) { return false; }

	bool Holster::CanAcceptObject(RE::TESBoundObject* a_obj) { return true; }

	bool NormalView::CanAcceptObject(RE::TESBoundObject* a_obj) { return true; }

	void Backpack::MoveGrabbed()
	{
		if (object->Is3DLoaded())
		{
			if (auto backpacknode = object->GetCurrent3D())
			{
				if (auto grabnode = backpacknode->GetObjectByName(g_backpack_grab_nodename))
				{
					RE::NiUpdateData ctx;

					auto handpos = RE::PlayerCharacter::GetSingleton()
									   ->GetVRNodeData()
									   ->LeftWandNode.get()
									   ->world.translate;

					backpacknode->local.translate = handpos;

					helper::FaceCamera(backpacknode);

					auto grabvector = backpacknode->local.rotate * grabnode->local.translate;

					backpacknode->local.translate -= grabvector;

					backpacknode->Update(ctx);
				}
			}
		}
	}

	/* Helper functions for managing rollover text */
	void Controller::ResetRolloverPosition()
	{
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

	void Controller::OverrideRolloverPosition()
	{
		auto pcvr = PlayerCharacter::GetSingleton()->GetVRNodeData();

		if (auto rollover_node = pcvr->RoomNode->GetObjectByName(g_rollover_nodename))
		{
			auto hand = pcvr->LeftWandNode;

			auto desired_world =
				hand->world.translate + hand->world.rotate * g_rollover_default_hand_pos;

			rollover_node->local.translate = rollover_node->parent->world.rotate.Transpose() *
				(desired_world - rollover_node->parent->world.translate);

			rollover_node->local.rotate = rollover_node->parent->world.rotate.Transpose() *
				hand->world.rotate * g_rollover_default_hand_rot;

			NiUpdateData ctx;
			rollover_node->Update(ctx);
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

	void Controller::MoveActivator(RE::TESObjectREFR* a_objref, bool a_enabled)
	{
		/* this is a total hack job but I can't figure out how to change collision layer on phantom shapes
		* or move rigidbodies without using MoveTo() */
		constexpr const char* collision_node_name = "CollisionNode";

		if (auto root = a_objref->GetCurrent3D())
		{
			if (auto collider = root->GetObjectByName(collision_node_name))
			{
				if (a_enabled)
				{
					auto handloc = PlayerCharacter::GetSingleton()
									   ->GetVRNodeData()
									   ->RightWandNode->world.translate;
					collider->local.translate = collider->parent->world.rotate.Transpose() *
						(handloc - collider->parent->world.translate);

					_DEBUGLOG("enabling activation collider for {}", a_objref->GetName());
				}
				else
				{
					collider->local.translate.z = 1000;
					_DEBUGLOG("enabling activation collider for {}", a_objref->GetName());
				}
			}
		}
	}

	/* Extradata helper functions */
	uint16_t SetOrGetID(RE::TESObjectREFR* a_obj)
	{
		if (auto existing_ID = a_obj->extraList.GetByType<RE::ExtraUniqueID>())
		{
			return existing_ID->uniqueID;
		}
		else
		{
			auto temp = new RE::ExtraUniqueID();
			temp->uniqueID = g_ID_upper;
			a_obj->extraList.Add(temp);
			return g_ID_upper;
		}
	}

	void AddTransformData(RE::ExtraDataList* a_edl, RE::NiTransform& a_transform)
	{
		auto temp = new RE::ExtraEditorRefMoveData();
		temp->realLocation = a_transform.translate;
		a_transform.rotate.ToEulerAnglesXYZ(temp->realAngle);
		a_edl->Add(temp);
	}
}

namespace RE
{
	ExtraEditorRefMoveData::~ExtraEditorRefMoveData() = default;
	ExtraDataType ExtraEditorRefMoveData::GetType() const
	{
		return ExtraDataType::kEditorRefMoveData;
	}

	ExtraWeaponAttackSound::~ExtraWeaponAttackSound() = default;  // 00

	// override (BSExtraData)
	ExtraDataType ExtraWeaponAttackSound::GetType() const
	{
		return RE::ExtraDataType::kWeaponAttackSound;
	}

	ExtraWorn::~ExtraWorn() = default;  // 00

	// override (BSExtraData)
	ExtraDataType ExtraWorn::GetType() const { return ExtraDataType::kWorn; }
}
