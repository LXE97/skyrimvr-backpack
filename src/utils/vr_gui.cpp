/** simple MVC GUI
 */
#include "vr_gui.h"

namespace vr_gui
{
	bool Hand::Update()
	{
		// check player 3d loaded
		if (auto pc = RE::PlayerCharacter::GetSingleton()->Get3D(false); pc)
		{
			// update transform
			if (auto hand_node = pc->GetObjectByName(vrinput::kControllerNodeName[isLeft]);
				hand_node)
			{
				node = hand_node;
				transform = &node->world;

				//query HIGGS state
				if (g_higgsInterface->IsHandInGrabbableState(isLeft)) { state = State::kReady; }
				else if (g_higgsInterface->GetGrabbedObject(isLeft)) { state = State::kGrabbing; }
				else { state = State::kWeapon; }

				return true;
			}
		}
		node = nullptr;
		transform = nullptr;
		return false;
	}

	void Controller::Init()
	{
		// create right (0) and left hands
		hands = { Hand(false), Hand(true) };

		// Populate object references
	}

	void Controller::DebugTestBackpack() {}

	void Controller::OnGameLoad()
	{
		// Parse cosave for items in backpacks

		// Clear backpacks and create player's
		hovered_map.clear();
	}

	void OnBackpackEquipped()
	{
		// Manage any NPC backpacks
	}

	void Controller::Update()
	{
		// only do these if there is at least one open backpack
		for (const auto& bp : backpacks)
		{
			if (bp->GetState() != Backpack::State::kClosed)
			{
				void UpdateAnimations();
				void HandleInput();
				void OverrideHUD();
				return;
			}
		}

		button_queue.clear();

		void HandleDeletionQueue();
		void HandleEvents();
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
			if (widget->GetParent()) { widget->GetParent()->RemoveChild(widget); }
		}
		widgets_to_delete.clear();
	}

	void Controller::HandleHUDOverrides()
	{
		static Widget* prev_override = nullptr;

		// Reset when no more overrides to do
		if (activator_overrides.empty() && prev_override != nullptr)
		{
			prev_override = nullptr;
			ClearHUDOverride();
		}
		else if (activator_overrides.back().owner != prev_override)
		{
			prev_override = activator_overrides.back().owner;
			SetHUDOverride(activator_overrides.back());
		}
	}

	void Controller::HandleInput()
	{
		// 1. Hover/ collision checking
		for (auto& hand : hands)
		{
			// Get updated device state
			if (!hand.Update()) { continue; }

			for (auto& bp : backpacks)
			{
				if (bp->GetState() == Backpack::State::kOpen ||
					// allow interactions with grabbed backpacks via the hand that is not grabbing
					(bp->GetState() == Backpack::State::kGrabbed && bp->GetGrabbedBy() != &hand))
				{
					// Copy currently hovered children for this hand and backpack
					auto&                cur_list = hovered_map[&hand][bp.get()];
					std::vector<Widget*> prev_list = cur_list;

					// Rebuild list of currently hovered widgets, and dispatch any newly hovered events
					cur_list.clear();
					if (bp->TestOverlap(hand)) { TraverseCollision(*bp, hand, cur_list); }

					// Any widgets in prev but not cur must have transitioned from hovered -> inactive state.
					// This also covers any widgets scheduled for deletion because they have been disabled
					// and weren't checked for collision
					for (Widget* w : prev_list)
					{
						auto it = std::find(cur_list.begin(), cur_list.end(), w);
						if (it == cur_list.end()) { w->OnHover(false, hand); }
					}

					// cleanup inactive backpacks from the global hover map
					// TODO- do this in backpack state change logic instead
					//if (cur_list.empty()) { hovered_map[&hand].erase(bp.get()); }
				}
				else if (bp->GetState() == Backpack::State::kGrabbed)
				{
					bp->MoveTo(*hand.GetTransform());
				}
				else  // backpack is inactive - its hover map was erased during state transition
				{}
			}
		}

		// 2. Button handling
		for (auto& event : button_queue)
		{
			// Dispatch the button to the highest priority widget that is hovered
			// only look in the first Backpack with hovered widgets for this hand- unlikely to have overlapping backpacks
			Hand* hand = &hands[event.isLeft];
			auto& hovered_this_hand = hovered_map[hand];

			for (auto& [bp, list] : hovered_this_hand)
			{
				if (!list.empty())
				{
					std::sort(list.begin(), list.end(),
						[](Widget* a, Widget* b) { return a->GetPriority() < b->GetPriority(); });
					list[0]->OnClick(event.down, event.isLeft, event.button);
					break;
				}
			}
		}
	}

	void Controller::TraverseCollision(
		Widget& a_parent, const Hand& a_hand, std::vector<Widget*>& a_hover_list)
	{
		for (auto& child : a_parent.GetChildren())
		{
			// All conditions for determining whether a widget is active are here
			if (child->IsEnabled() && child->HandStateFilter() && child->TestOverlap(a_hand))
			{
				if (!child->IsHovered()) { child->OnHover(true, a_hand); }
				a_hover_list.push_back(child.get());
			}
			// always visit the whole tree even if parent is disabled
			TraverseCollision(*child, a_hand, a_hover_list);
		}
	}

	void Controller::PushActivatorOverride(ActivatorOverride&& a_data)
	{
		activator_overrides.emplace_back(std::move(a_data));
	}

	void Controller::RemoveActivatorOverride(Widget* a_owner)
	{
		auto it = std::find_if(activator_overrides.begin(), activator_overrides.end(),
			[&](const ActivatorOverride& a) { return a.owner == a_owner; });
		if (it != activator_overrides.end()) { activator_overrides.erase(it); }
	}

	void Controller::ClearHUDOverride() {}

	void Controller::SetHUDOverride(ActivatorOverride& a_data) {}

	Widget::Widget(Widget* a_parent, NiNode* a_parent_node, NiTransform&& a_local,
		NiPoint3 a_halfextents, float a_radius) :
		parent(a_parent),
		parent_node(a_parent_node),
		local(std::move(a_local)),
		extents(a_halfextents),
		radius(a_radius)
	{
		if (!parent_node && parent) { parent_node = parent->parent_node; }
	}

	Widget::Widget(Widget* a_parent, NiNode* a_parent_node, NiTransform& a_local,
		NiPoint3 a_halfextents, float a_radius) :
		parent(a_parent),
		parent_node(a_parent_node),
		local(a_local),
		extents(a_halfextents),
		radius(a_radius)
	{
		if (!parent_node && parent) { parent_node = parent->parent_node; }
	}

	void Widget::RemoveChild(Widget* a_child)
	{
		auto it = std::remove_if(children.begin(), children.end(),
			[a_child](const std::unique_ptr<Widget>& ptr) { return ptr.get() == a_child; });

		if (it != children.end()) children.erase(it, children.end());
	}

	bool Widget::TestOverlap(const Hand& a_Hand) const
	{
		if (auto t = a_Hand.GetTransform(); t)
		{
			auto w = GetWorld();
			if (helper::IntersectSphereSphere(
					t->translate, a_Hand.GetRadius(), w.translate, w.scale * radius))
			{
				return true;
				return helper::IntersectOBBOBB(*t, *a_Hand.GetExtents(), w, extents * w.scale);
			}
		}
		return false;
	}

	void Widget::OnClick(bool a_activate, const Hand& a_hand, MenuAction a_action)
	{
		SKSE::log::trace("click event received on widget");
	}

	void Widget::OnHover(bool a_activate, const Hand& a_hand)
	{
		SKSE::log::trace("click event received on widget");

		hover_state = a_activate;
		if (a_activate) { AddEffect("DebugSphere.nif"); }
		else {}
	}

	const Widget* Widget::GetRoot() const
	{
		if (parent == nullptr) { return this; }
		else { return parent->GetParent(); }
	}

	NiTransform Widget::GetWorld(int depth) const
	{
		if (depth > MAX_DEPTH)
		{
			// Return identity transform when maximum recursion depth exceeded
			return NiTransform();
		}

		if (parent_node)
		{
			// If this widget is attached to a 3D node, return its world transform
			return local * parent_node->world;
		}
		else if (parent)
		{
			// otherwise call GetWorld on the parent
			return local * parent->GetWorld(depth + 1);
		}
		else { return local; }
	}

	void Widget::AddEffect(const std::string a_path)
	{
		// get object reference first
		if (auto root = GetRoot())
		{
			if (auto rv = dynamic_cast<const RootView*>(root))
			{
				if (auto obj = rv->GetObjRef())
				{
					// decide which node to attach to
					NiAVObject* target {};
                    NiTransform t {};
					if (auto this3d = model->Get3D()) { target = this3d; }
					else if (parent_node) { target = parent_node; t = local;}
					else { target = obj->Get3D(); }
					visual_effects.emplace_back(
						art_addon::ArtAddon::Make("DebugSphere.nif", obj, target, t));
				}
			}
		}
	}

	RootView::RootView(TESObjectREFR* a_rootobj) :
		Widget(nullptr,                                 // RootView has no parent widget
			nullptr,                                    // get 3D parent from object ref
			NiTransform(), NiPoint3(10.f, 10.f, 10.f),  // default box
			100.f                                       // default radius
			),
		rootobj(a_rootobj)
	{
		if (auto node = rootobj->Get3D()) { parent_node = node->AsNode(); }
	}

	// Broad phase only
	bool RootView::TestOverlap(Hand& a_Hand)
	{
		if (auto t = a_Hand.GetTransform(); t)
		{
			auto world = GetWorld();
			return helper::IntersectSphereSphere(
				t->translate, a_Hand.GetRadius(), world.translate, world.scale * radius);
		}
		return false;
	}

	Backpack::Backpack(TESObjectREFR* a_rootobj, TESObjectREFR* a_owner) :
		RootView(a_rootobj),
		owner(a_owner)
	{

    }

    

	void Backpack::MoveTo(const NiTransform& a_newtransform) {}

}