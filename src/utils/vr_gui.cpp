#include "vr_gui.h"

#include "helper_game.h"
#include "helper_math.h"

namespace vr_gui
{
	namespace
	{
		bool BlockKeypress(const vrinput::ModInputEvent& e) { return false; }

		void DrawBox(art_addon::ArtAddon* box, const RE::NiPoint3& dimensions)
		{
			if (box)
			{
				RE::NiAVObject* geom = box->Get3D();
				for (int i : { 0, 1 })
					for (int j : { 0, 1 })
						for (int k : { 0, 1 })
						{
							char name[4] = { char('0' + i), char('0' + j), char('0' + k), 0 };

							if (auto node = geom->GetObjectByName(name))
							{
								float x = (i ? +dimensions.x : -dimensions.x);
								float y = (j ? +dimensions.y : -dimensions.y);
								float z = (k ? +dimensions.z : -dimensions.z);

								node->local.translate = { x, y, z };
							}
						}
			}
		}
	}

	const NiTransform Hand::GetTransform() const
	{
		if (transform) { return *transform * offset; }
		else
		{
			return offset;
		}
	}

	bool Hand::Init()
	{
		if (initialized) { return true; }
		// check player 3d loaded
		if (auto pc = RE::PlayerCharacter::GetSingleton()->Get3D(false); pc)
		{
			// link transforms
			if (auto hand_node = pc->GetObjectByName(vrinput::kControllerNodeName[isLeft]);
				hand_node)
			{
				node = hand_node;
				transform = &node->world;
				initialized = true;
				offset.translate = { 0, -0.5, 5 };
				return true;
			}
		}
		node = nullptr;
		transform = nullptr;
		initialized = false;
		return false;
	}

	bool Hand::Update()
	{
		if (Init())
		{
			//query HIGGS state
			if (g_higgsInterface->IsHandInGrabbableState(isLeft)) { state = State::kReady; }
			else if (g_higgsInterface->GetGrabbedObject(isLeft)) { state = State::kGrabbing; }
			else
			{
				state = State::kWeapon;
			}
			return true;
		}
		return false;
	}

	void Controller::Cleanup()
	{
		windows.clear();
		activator_overrides.clear();
		widgets_to_delete.clear();
		hovered_map.clear();
	}

	void Controller::Init()
	{
		SKSE::log::trace("Controller init");

		windows.clear();
		activator_overrides.clear();
		widgets_to_delete.clear();
		hovered_map.clear();

		auto* setting = RE::GetINISetting("fActivatePickLength:Interface");
		if (setting) factivatepicklength_default = setting->data.f;

		g_higgsInterface->GetSettingDouble("FarCastDistance", FarCastDistance);
		g_higgsInterface->GetSettingDouble("NearCastDistance", NearCastDistance);

		// populate references
		if (auto form = helper::GetForm(kActivatorID, kPluginName))
		{
			activator_obj = form->AsReference();
			activator_default_base = activator_obj->GetObjectReference();
			activator_obj->SetActivationBlocked(true);
		}
		else
		{
			SKSE::log::error("ERROR couldn't get activator objref");
		}
		if (auto form = helper::GetForm(kMarkerModspaceID, kPluginName))
		{
			modspacemarker_obj = form->AsReference();
		}
		else
		{
			SKSE::log::error("ERROR couldn't get modspace marker objref");
		}

		if (auto form = TESForm::LookupByID(0x7))
		{
			player_form_ref = form;
			SKSE::log::trace("player_form_ref {}", fmt::ptr(player_form_ref));
		}
		else
		{
			SKSE::log::error("ERROR couldn't get player ref");
		}

		if (RE::PlayerCharacter::GetSingleton()->Is3DLoaded())
		{
			auto pcvr = RE::PlayerCharacter::GetSingleton()->GetVRNodeData();
			auto hand = pcvr->RightWandNode;
			if (auto rollover = pcvr->RoomNode->GetObjectByName(kRolloverNodeName))
			{
				rollover_default_hand = rollover->local;
				rollover_default_hand_pos = rollover->local.translate;
				rollover_default_hand_rot = rollover->local.rotate;

				SKSE::log::trace("default rollover transform:");
				helper::PrintVec(rollover_default_hand_pos);
			}
			else
			{
				SKSE::log::error("ERROR couldn't get rollover node");
			}
		}

		ReleaseInputBlock(true);

		// create right (0) and left hands
		hands = { Hand(false), Hand(true) };

		// register for input events
		vrinput::AddCallback(InputEventHandlerStatic, settings.primary, vrinput::Hand::kRight,
			vrinput::ActionType::kPress);
		vrinput::AddCallback(InputEventHandlerStatic, settings.secondary, vrinput::Hand::kRight,
			vrinput::ActionType::kPress);
		vrinput::AddCallback(InputEventHandlerStatic, vr::EVRButtonId::k_EButton_DPad_Up,
			vrinput::Hand::kRight, vrinput::ActionType::kPress);
		vrinput::AddCallback(InputEventHandlerStatic, vr::EVRButtonId::k_EButton_DPad_Down,
			vrinput::Hand::kRight, vrinput::ActionType::kPress);
		vrinput::AddCallback(InputEventHandlerStatic, settings.primary, vrinput::Hand::kLeft,
			vrinput::ActionType::kPress);
		vrinput::AddCallback(InputEventHandlerStatic, settings.secondary, vrinput::Hand::kLeft,
			vrinput::ActionType::kPress);
		vrinput::AddCallback(InputEventHandlerStatic, vr::EVRButtonId::k_EButton_DPad_Up,
			vrinput::Hand::kLeft, vrinput::ActionType::kPress);
		vrinput::AddCallback(InputEventHandlerStatic, vr::EVRButtonId::k_EButton_DPad_Down,
			vrinput::Hand::kLeft, vrinput::ActionType::kPress);
	}

	void Controller::Update()
	{
		// only run updates if there's at least one enabled window
		bool do_update = false;

		const auto                         now = std::chrono::steady_clock::now();
		const std::chrono::duration<float> elapsed = now - last_update_time;
		const float                        delta = elapsed.count();
		last_update_time = now;

		for (const auto& w : windows)
		{
			if (w->IsEnabled())
			{
				w->UpdateImpl(delta);
				do_update = true;
			}
		}
		if (do_update)
		{
			HandleInput();
			HandleEvents();
		}
		else
		{
			activator_overrides.clear();
		}

		//HandleHUDOverrides happens in the PostWandUpdate
		HandleDeletionQueue();
	}

	bool Controller::InputEventHandlerStatic(const vrinput::ModInputEvent& e)
	{ return GetSingleton()->InputEventHandler(e); }

	bool Controller::InputEventHandler(const vrinput::ModInputEvent& e)
	{
		ButtonEvent temp;
		temp.down = (bool)e.button_state;
		temp.isLeft = (bool)e.device;
		if (e.button_ID == settings.primary) temp.button = MenuAction::kPrimary;
		if (e.button_ID == settings.secondary) temp.button = MenuAction::kSecondary;
		if (e.button_ID == vr::EVRButtonId::k_EButton_DPad_Up) temp.button = MenuAction::kScrollUp;
		if (e.button_ID == vr::EVRButtonId::k_EButton_DPad_Down)
			temp.button = MenuAction::kScrollDown;

		button_queue.push_back(temp);

		return true;
	}

	void Controller::MarkForDelete(Widget* w)
	{
		if (w)
		{
			w->SetEnabled(false);
			widgets_to_delete.push_back(w);
		}
	}

	void Controller::HandleDeletionQueue()
	{
		for (auto* widget : widgets_to_delete)
		{
			if (IsValid(widget))
			{
				if (widget->GetParent())
				{
					widget->GetParent()->RemoveChild(widget);
					//remove dangling reference from hover map
					for (auto& [hand, windowMap] : hovered_map)
					{
						for (auto& [window, widgets] : windowMap) { std::erase(widgets, widget); }
					}
				}
				else
				{
					RemoveWindow(static_cast<Window*>(widget));
				}
			}
		}
		widgets_to_delete.clear();
	}

	bool Controller::IsValid(Widget* a_target) const
	{
		for (auto& root : windows)
		{
			if (IsValid(root.get(), a_target)) { return true; }
		}
		return false;
	}

	bool Controller::IsValid(Widget* a_root, Widget* a_target) const
	{
		if (a_root == a_target) { return true; }

		for (auto& w : a_root->children)
		{
			if (IsValid(w.get(), a_target)) { return true; }
		}

		return false;
	}

	void Widget::RemoveChild(Widget* a_child)
	{
		auto it = std::find_if(children.begin(), children.end(),
			[a_child](const std::unique_ptr<Widget>& ptr) { return ptr.get() == a_child; });

		if (it != children.end()) { children.erase(it); }
	}

	void Controller::RemoveWindow(Window* a_del)
	{
		auto it = std::find_if(windows.begin(), windows.end(),
			[a_del](const std::unique_ptr<Window>& ptr) { return ptr.get() == a_del; });

		if (it != windows.end())
		{
			for (auto& hand : hands)
			{
				if (it->get()->IsHovered(hand.IsLeft())) { ReleaseInputBlock(); }
			}
			RemoveActivatorOverride(a_del);
			windows.erase(it);
			for (auto& hand : hands) { hovered_map[&hand].erase(a_del); }
		}

		if (windows.empty())
		{
			SKSE::GetTaskInterface()->AddTask([this]() {
				if (activator_obj) { activator_obj->MoveTo(modspacemarker_obj); }
			});
		}
	}

	void Controller::HandleHUDOverrides()
	{
		static Widget* prev_override = nullptr;

		if (activator_obj && modspacemarker_obj)
		{
			// Reset when no more overrides to do
			if (activator_overrides.empty())
			{
				if (prev_override != nullptr)
				{
					prev_override = nullptr;
					//SKSE::GetTaskInterface()->AddTask([this]() { ClearHUDOverride(); });
					ClearHUDOverride();
				}
			}
			// New override to set
			else if (activator_overrides.back().owner != prev_override)
			{
				prev_override = activator_overrides.back().owner;
				SetHUDOverride(activator_overrides.back());
			}
			// No changes in override data but override is still active, just do position update
			else
			{
				// move to follow main hand (Activation hand)
				//SKSE::GetTaskInterface()->AddTask([this]() { ToggleActivator(true); });
				ToggleActivator(true);
				if (activator_overrides.back().isLeft)
				{
					// override the rollover UI position
					auto pc = PlayerCharacter::GetSingleton();

					if (auto rollover_node = pc->RoomNode->GetObjectByName(kRolloverNodeName))
					{
						NiUpdateData ctx;
						auto         hand = pc->LeftWandNode;

						auto desired_world = hand->world * rollover_default_hand;

						rollover_node->local =
							rollover_node->parent->world.Invert() * desired_world;

						rollover_node->Update(ctx);
					}
				}
			}
		}
	}

	void Controller::HandleInput()
	{
		// 1. Hover/ collision checking
		for (auto& hand : hands)
		{
			// Get updated device state, skip if failed
			if (!hand.Update()) { continue; }

			for (auto& root : windows)
			{
				if (root->IsEnabled())
				{
					// Copy currently hovered children for this hand and Window
					auto&                cur_list = hovered_map[&hand][root.get()];
					std::vector<Widget*> prev_list = cur_list;

					// Rebuild list of currently hovered widgets, and dispatch any newly hovered events
					cur_list.clear();
					if (root->TestOverlap(hand))
					{
						if (!root->IsHovered(hand.IsLeft())) root->OnHoverImpl(true, hand);
						TraverseCollision(*root, hand, cur_list);
					}
					else
					{
						if (root->IsHovered(hand.IsLeft())) root->OnHoverImpl(false, hand);
					}

					// Any widgets in prev but not cur must have transitioned from hovered -> inactive state.
					// This also covers any widgets scheduled for deletion because they have been disabled
					// and weren't checked for collision
					for (Widget* w : prev_list)
					{
						auto it = std::find(cur_list.begin(), cur_list.end(), w);
						if (it == cur_list.end()) { w->OnHoverImpl(false, hand); }
					}

					// cleanup inactive windows from the global hover map
					if (cur_list.empty()) { hovered_map[&hand].erase(root.get()); }
				}
			}
		}
	}

	void Controller::HandleEvents()
	{
		for (auto& event : button_queue)
		{
			// Dispatch the button to the highest priority widget that is hovered
			// only look in the first window with hovered widgets for this hand- unlikely to have overlapping Windows
			Hand* hand = &hands[event.isLeft];
			auto& hovered_this_hand = hovered_map[hand];

			for (auto& [window, list] : hovered_this_hand)
			{
				if (!list.empty())
				{
					std::sort(list.begin(), list.end(),
						[](Widget* a, Widget* b) { return a->GetPriority() < b->GetPriority(); });
					list[0]->OnClick(event.down, *hand, event.button);
					break;
				}
			}
		}
		button_queue.clear();
	}

	void Controller::TraverseCollision(
		Widget& a_parent, Hand& a_hand, std::vector<Widget*>& a_hover_list)
	{
		for (auto& child : a_parent.GetChildren())
		{
			// All conditions for determining whether a widget is active are here
			if (child->IsEnabled() && child->HandStateFilter() && child->TestOverlap(a_hand))
			{
				if (!child->IsHovered(a_hand.IsLeft())) { child->OnHoverImpl(true, a_hand); }
				a_hover_list.push_back(child.get());
			}
			// always visit the whole tree even if parent is disabled
			// TODO: why?
			TraverseCollision(*child, a_hand, a_hover_list);
		}
	}

	void Controller::PushActivatorOverride(ActivatorOverride&& a_data)
	{ activator_overrides.emplace_back(std::move(a_data)); }

	void Controller::RemoveActivatorOverride(Widget* a_owner)
	{
		auto it = std::find_if(activator_overrides.begin(), activator_overrides.end(),
			[&](const ActivatorOverride& a) { return a.owner == a_owner; });
		if (it != activator_overrides.end()) { activator_overrides.erase(it); }
	}

	void Controller::RemoveActivatorOverride(Widget* a_owner, bool a_isLeft)
	{
		auto it = std::find_if(activator_overrides.begin(), activator_overrides.end(),
			[&](const ActivatorOverride& a) { return a.owner == a_owner && a.isLeft == a_isLeft; });
		if (it != activator_overrides.end()) { activator_overrides.erase(it); }
	}

	void Controller::RemoveActivatorOverride(Window* a_parent)
	{
		auto it = std::remove_if(activator_overrides.begin(), activator_overrides.end(),
			[&](const ActivatorOverride& a) { return a.parent == a_parent; });
		if (it != activator_overrides.end())
		{
			activator_overrides.erase(it, activator_overrides.end());
		}
	}

	void Controller::AcquireInputBlock()
	{
		if (++input_block_counter == 1)
		{
			SKSE::log::trace("---------------------------------------------------- BLOCKING");
			// disable HIGGS, Spellwheel, etc
			for (bool isLeft : { true, false }) { vrinput::BlockAxis(isLeft); }
			g_higgsInterface->GetSettingDouble("FarCastDistance", FarCastDistance);
			g_higgsInterface->GetSettingDouble("NearCastDistance", NearCastDistance);
			g_higgsInterface->SetSettingDouble("FarCastDistance", 0.0000001);
			g_higgsInterface->SetSettingDouble("NearCastDistance", 0.0000001);
			g_vrikInterface->beginGestureProfile();
			auto* setting = RE::GetINISetting("fActivatePickLength:Interface");
			if (setting) setting->data.f = 10.f;
		}
	}

	void Controller::ReleaseInputBlock(bool a_force)
	{
		if (a_force || --input_block_counter == 0)
		{
			input_block_counter = 0;
			SKSE::log::trace("---------------------------------------------------- RELEASING");
			// re enable everything
			for (bool isLeft : { true, false }) { vrinput::UnBlockAxis(isLeft); }
			g_higgsInterface->SetSettingDouble("FarCastDistance", FarCastDistance);
			g_higgsInterface->SetSettingDouble("NearCastDistance", NearCastDistance);
			g_vrikInterface->beginGestureProfile();
			auto* setting = RE::GetINISetting("fActivatePickLength:Interface");
			if (setting) setting->data.f = Controller::GetSingleton()->factivatepicklength_default;
		}
	}

	void Controller::ClearExtraData(RE::TESObjectREFR* a_obj)
	{
		if (a_obj->extraList.HasType(RE::ExtraDataType::kEnchantment))
		{
			a_obj->extraList.RemoveByType(RE::ExtraDataType::kEnchantment);
		}
		if (a_obj->extraList.HasType(RE::ExtraDataType::kOwnership))
		{
			auto own = a_obj->extraList.GetByType<RE::ExtraOwnership>();
			own->owner = player_form_ref;
		}
		else
		{
			auto own = RE::ExtraOwnership::Create<RE::ExtraOwnership>();
			own->owner = player_form_ref;
			a_obj->extraList.Add(own);
		}
	}

	void Controller::ClearHUDOverride()
	{
		activator_obj->SetObjectReference(activator_default_base);
		//  reset extradata

		ClearExtraData(activator_obj);
		ToggleActivator(false);
	}

	void Controller::SetHUDOverride(ActivatorOverride& a_data)
	{
		if (!a_data.base) return;

		// The rollover text doesn't refresh until you move off this object, so disable it
		// it will be enabled on the next update
		ToggleActivator(false);

		ClearExtraData(activator_obj);

		if (a_data.text.empty())
		{
			// change the displayed item in the rollover UI
			activator_obj->SetObjectReference(a_data.base);
			activator_obj->SetDisplayName(a_data.base->GetName(), false);

			if (auto e = a_data.extradata)
			{
				if (e->HasType(RE::ExtraDataType::kEnchantment))
				{
					auto ench = a_data.extradata->GetByType<RE::ExtraEnchantment>();
					auto ench_copy = RE::ExtraEnchantment::Create<RE::ExtraEnchantment>();
					ench_copy->charge = ench->charge;
					ench_copy->enchantment = ench->enchantment;
					activator_obj->extraList.Add(ench_copy);
				}
				if (e->HasType(RE::ExtraDataType::kOwnership))
				{
					auto own = a_data.extradata->GetByType<RE::ExtraOwnership>();
					if (activator_obj->extraList.HasType(RE::ExtraDataType::kOwnership))
					{
						auto act_own = activator_obj->extraList.GetByType<RE::ExtraOwnership>();
						act_own->owner = own->owner;
					}
					else
					{
						auto own_copy = RE::ExtraOwnership::Create<RE::ExtraOwnership>();
						own_copy->owner = own->owner;
						activator_obj->extraList.Add(own_copy);
					}
				}
				if (e->HasType(RE::ExtraDataType::kPoison))
				{
					//TODO: poisoned indicator
				}
			}
		}
		else
		{
			activator_obj->SetObjectReference(activator_default_base);
			activator_obj->SetDisplayName(a_data.text, true);
		}
	}

	void Controller::ToggleActivator(bool a_enabled)
	{
		constexpr const char* collision_node_name = "CollisionNode";

		if (auto root = activator_obj->GetCurrent3D())
		{
			if (auto collider = root->GetObjectByName(collision_node_name))
			{
				RE::NiUpdateData ctx;
				if (a_enabled)
				{
					auto handloc = PlayerCharacter::GetSingleton()->RightWandNode->world.translate;

					collider->local.translate = collider->parent->world.rotate.Transpose() *
						(handloc - collider->parent->world.translate);

					collider->Update(ctx);
				}
				else
				{
					collider->local.translate.z += 500;
					collider->Update(ctx);

					// fix the rotation, no better place to put this
					root->world.rotate = NiMatrix3();
				}
			}
		}
	}

	void Controller::ShowHitboxes(bool show)
	{
		for (auto& win : windows) win->ShowHitboxes(show);
	}

	Widget::Widget(Widget* a_parent, NiTransform a_local, NiPoint3 a_halfextents, float a_radius) :
		parent(a_parent),
		local(std::move(a_local)),
		extents(a_halfextents),
		radius(a_radius)
	{}

	void Widget::ShowHitboxes(bool show)
	{
		DrawExtents(show);

		for (auto& child : children) { child->ShowHitboxes(show); }
	}

	void Widget::DrawExtents(bool show)
	{
		using namespace art_addon;
		if (show)
		{
			AddModel("DrawExtents.nif", true, [extents = this->extents](ArtAddon* box) {
				DrawBox(box, extents);
				SKSE::log::trace(
					"showing extents on widget: {} {} {}", extents.x, extents.y, extents.z);
			});
			AddModel("DebugSphere.nif", true, [radius = this->radius](ArtAddon* sphere) {
				if (sphere && sphere->Get3D()) sphere->Get3D()->local.scale = radius;
			});
		}
		else
		{
			visual_effects.clear();
		}
	}

	void Window::DrawExtents(bool show)
	{
		SKSE::log::trace("{}ing hitboxes on window ", show ? "show" : "hid");
		using namespace art_addon;
		if (show)
		{
			AddModel("DebugSphere.nif", true, [radius = this->radius](ArtAddon* sphere) {
				if (sphere && sphere->Get3D()) { sphere->Get3D()->local.scale = radius; }
			});
		}
		else
		{
			visual_effects.clear();
		}
	}

	bool Widget::TestOverlap(Hand& a_Hand) const
	{
		// TODO: rewrite to use a downward pass instead of GetWorld every time
		auto t = a_Hand.GetTransform();
		auto w = GetWorld();
		// broad phase
		if (helper::IntersectSphereSphere(
				t.translate, a_Hand.GetRadius(), w.translate, w.scale * radius))
		{  // narrow phase
			return helper::IntersectOBBOBB(t, *a_Hand.GetExtents() * t.scale, w, extents * w.scale);
		}

		return false;
	}

	void Widget::OnClick(bool a_activate, Hand& a_hand, MenuAction a_action)
	{
		SKSE::log::debug("click event received on widget: {} {}",
			a_hand.IsLeft() ? "left" : "right", a_activate ? "down" : "up");
	}

	void Widget::OnHoverImpl(bool activate, Hand& hand)
	{
		hover_state[hand.IsLeft()] = activate;
		OnHover(activate, hand);

		for (auto& b : behaviors)
		{
			if (!b->IsMarkedForRemoval()) { b->OnHover(activate, hand); }
		}
	}

	void Widget::Hide()
	{
		if (auto* node = Get3D()) { node->SetAppCulled(true); }
		hide = true;
		for (auto& w : children) { w->Hide(); }
	}

	void Widget::Show()
	{
		if (auto* node = Get3D()) { node->SetAppCulled(false); }
		hide = false;
		for (auto& w : children) { w->Show(); }
	}

	void Widget::Resize(float a_scale)
	{
		local.scale = a_scale;

		// Apply the new scale to the model. Since it has a different parent tree than the Widget, we need
		// to accumulate the Widget scale up the tree
		if (auto* node = Get3D())
		{
			RE::NiUpdateData ctx;
			node->local.scale = GetLocalToRoot().scale;
			node->Update(ctx);
		}

		for (auto& w : children) { w->UpdateModelTransform(); }
	}

	/* Set new local translation */
	void Widget::MoveTo(NiPoint3 a_translate_local)
	{
		local.translate = a_translate_local;
		if (auto* node = Get3D())
		{
			RE::NiUpdateData ctx;
			auto             model_local = node->parent->world.Invert() * GetWorld();
			node->local.translate = model_local.translate;
			node->Update(ctx);
		}

		for (auto& w : children) { w->UpdateModelTransform(); }
	}

	void Widget::OnHover(bool a_activate, Hand& a_hand) {}

	void Widget::Update(float delta) {}

	void Widget::UpdateImpl(float delta)
	{
		Update(delta);

		for (auto& b : behaviors)
		{
			if (!b->IsMarkedForRemoval()) { b->Update(delta); }
		}

		for (auto& child : children) { child->UpdateImpl(delta); }

		std::erase_if(behaviors, [](const auto& b) { return b->IsMarkedForRemoval(); });
	}

	const Window* Widget::GetWindow() const
	{
		if (parent == nullptr) { return static_cast<const Window*>(this); }
		else
		{
			return parent->GetWindow();
		}
	}

	Window* Widget::GetWindow()
	{
		if (parent == nullptr) { return static_cast<Window*>(this); }
		else
		{
			return parent->GetWindow();
		}
	}

	NiTransform Widget::GetWorld(int depth) const
	{
		if (depth > MAX_DEPTH || !parent)
		{
			// Return identity transform when maximum recursion depth exceeded
			SKSE::log::error("Widget tree recursion exceeded");
			return NiTransform();
		}

		// call GetWorld on the parent widget until the root (Window) is reached
		return parent->GetWorld(depth + 1) * local;
	}

	NiTransform Window::GetWorld(int depth) const
	{
		if (depth > MAX_DEPTH)
		{
			// Return identity transform when maximum recursion depth exceeded
			SKSE::log::error("Widget tree recursion exceeded");
			return NiTransform();
		}

		if (auto* root = GetRootNode()) { return root->world * local; }

		return local;
	}

	NiTransform Widget::GetLocalToRoot() const
	{ return GetWindow()->GetWorld().Invert() * GetWorld(); }

	void Widget::UpdateModelTransform()
	{
		if (auto node = Get3D())
		{
			RE::NiUpdateData ctx;
			node->local = GetLocalToRoot();
			node->Update(ctx);
		}
	}

	void Widget::AddModel(const std::string& a_path, bool a_tempeffect,
		std::function<void(art_addon::ArtAddon*)> a_3DInitializedCallback)
	{
		// get object reference first
		if (auto root = GetWindow())
		{
			if (auto rv = dynamic_cast<const Window*>(root))
			{
				if (auto obj = rv->GetObjRef())
				{
					NiAVObject* target = rv->GetRootNode();
					NiTransform t = GetLocalToRoot();

					// Add the model
					if (a_tempeffect)
					{
						visual_effects.emplace_back(art_addon::ArtAddon::Make(
							a_path.c_str(), obj, target, t, a_3DInitializedCallback));
					}
					else
					{
						model = art_addon::ArtAddon::Make(
							a_path.c_str(), obj, target, t, a_3DInitializedCallback);
					}
				}
			}
		}
	}

	void Window::AddModel(const std::string& a_path, bool a_tempeffect,
		std::function<void(art_addon::ArtAddon*)> a_3DInitializedCallback)
	{
		RE::NiTransform t{};

		if (a_tempeffect)
		{
			visual_effects.emplace_back(art_addon::ArtAddon::Make(
				a_path.c_str(), rootobj, GetRootNode(), t, a_3DInitializedCallback));
		}
		else
		{
			model = art_addon::ArtAddon::Make(
				a_path.c_str(), rootobj, GetRootNode(), t, a_3DInitializedCallback);
		}
	}

	AttachedWindow::AttachedWindow(float a_radius, RE::TESObjectREFR* a_rootobj,
		RE::NiNode* a_parent_node, std::optional<RE::NiTransform> a_local) :
		Window(a_radius, a_rootobj, a_parent_node, a_local)
	{
		if (!parent_node)
		{
			if (auto node = rootobj->Get3D(false); node) { parent_node = node->AsNode(); }
		}
	}

	FloatingWindow::FloatingWindow(float a_radius, TESObjectREFR* a_rootobj,
		RE::NiTransform a_world, std::function<void(FloatingWindow*)> a_3DInitializedCallback) :
		Window(a_radius, a_rootobj, nullptr, a_world),
		InitializedCallback(a_3DInitializedCallback)
	{
		if (auto node = a_rootobj ? rootobj->Get3D(false) : nullptr; node)
		{
			auto initial = node->world.Invert() * a_world;
			auto callback = InitializedCallback;
			model = art_addon::ArtAddon::Make("effects/fxemptyobject.nif", rootobj, node, initial,
				[this, callback](art_addon::ArtAddon* a) {
					if (callback) { callback(this); }
				});
		}
	}

	void FloatingWindow::Update(float delta)
	{
		// Update absolute world position for windows that are not attached to an object
		// For FloatingWindow,  local  actually stores the desired world transform
		if (auto* node = Get3D())
		{
			auto ctx = RE::NiUpdateData();
			node->local = model->GetParent()->world.Invert() * local;
			node->Update(ctx);
		}
	}

	NiTransform FloatingWindow::GetWorld(int depth) const
	{
		if (depth > MAX_DEPTH)
		{
			// Return identity transform when maximum recursion depth exceeded
			SKSE::log::error("Widget tree recursion exceeded");
			return NiTransform();
		}

		return local;
	}

	NiNode* FloatingWindow::GetRootNode() const
	{
		auto* node = model ? model->Get3D() : rootobj->Get3D();
		return node->AsNode();
	}

	// Broad phase only
	bool Window::TestOverlap(Hand& a_Hand) const
	{
		auto t = a_Hand.GetTransform();

		auto world = GetWorld();
		return helper::IntersectSphereSphere(
			t.translate, a_Hand.GetRadius(), world.translate, world.scale * radius);
	}

	void Window::OnHover(bool a_activate, Hand& a_hand)
	{
		if (a_activate) { Controller::GetSingleton()->AcquireInputBlock(); }
		else
		{
			Controller::GetSingleton()->ReleaseInputBlock();
		}
	}

	void Container::Filter(FilterFunc f)
	{
		for (auto& child : children)
		{
			if (auto b = f(*child))
			{
				child->SetEnabled(true);
				child->Show();
			}
			else
			{
				child->SetEnabled(false);
				child->Hide();
			}
		}
	}

	void Container::Sort(SortFunc f)
	{
		std::sort(children.begin(), children.end(),
			[f](const auto& a, const auto& b) { return f(*a, *b); });
	}

	void GridContainer::Filter(FilterFunc f)
	{
		current_filter = f;
		Redraw();
	}

	void GridContainer::Sort(SortFunc f)
	{
		current_sort = f;
		Redraw();
	}

	void GridContainer::RemoveGridChild(Widget* a_child)
	{
		Controller::GetSingleton()->MarkForDelete(a_child);
		redraw_counter = 2;
	}

	void GridContainer::Scroll(int a_adjustment)
	{ scroll_position = std::clamp(scroll_position + a_adjustment, 0, GetVisibleRows()); }

	void GridContainer::Update(float delta)
	{
		if (redraw_counter > 0)
		{
			if (--redraw_counter == 0) { Redraw(); }
		}
		if (last_scroll_position != scroll_position)
		{
			Redraw();
			last_scroll_position = scroll_position;
		}
	}

	void GridContainer::Redraw()
	{
		if (current_sort) { Container::Sort(current_sort); }

		visible_index = 0;

		const float usable_width =
			extents.x * 2.0f - static_cast<float>(items_per_row - 1) * padding;

		item_radius = usable_width / static_cast<float>(items_per_row) * 0.5f;

		const float spacing = item_radius * 2.0f + padding;

		const float total_width = static_cast<float>(items_per_row) * item_radius * 2.0f +
			static_cast<float>(items_per_row - 1) * padding;

		const float start_x = -total_width * 0.5f + item_radius;
		const float start_z = extents.z - item_radius;

		const int first_visible_row = static_cast<int>(std::floor(scroll_position));
		const int visible_rows =
			static_cast<int>(std::floor((extents.z * 2.0f + padding) / spacing));

		for (auto& child : children)
		{
			if (current_filter && !current_filter(*child))
			{
				child->Hide();
				child->SetEnabled(false);
				continue;
			}

			const int row = visible_index / items_per_row;
			const int col = visible_index % items_per_row;

			const bool inside = row >= first_visible_row && row < first_visible_row + visible_rows;

			if (inside)
			{
				const int local_row = row - first_visible_row;

				const NiPoint3 position{ start_x + static_cast<float>(col) * spacing, 0.0f,
					start_z - static_cast<float>(local_row) * spacing };

				child->Resize(item_radius);
				child->MoveTo(position);
				child->Show();
				child->SetEnabled(true);
			}
			else
			{
				child->Hide();
				child->SetEnabled(false);
			}

			++visible_index;
		}
	}

	void Behavior::Update(float delta) {}
	void Behavior::OnHover(bool activate, Hand& hand) {}
}