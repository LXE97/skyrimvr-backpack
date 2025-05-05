/** simple MVC GUI
 */
#pragma once
#include "art_addon.h"
#include "higgsinterface001.h"
#include "vrinput.h"

#include <unordered_set>

namespace vr_gui
{
	using namespace RE;
	class Widget;
	class Item;
	class Hand;

	enum class MenuAction
	{
		kPrimary,
		kSecondary,
		kScrollUp,
		kScrollDown
	};

	// Base View class
	class Widget
	{
	public:
		Widget(Widget* a_parent, NiNode* a_parent_node, NiTransform&& a_local,
			NiPoint3 a_halfextents = NiPoint3{}, float a_radius = 1.f);
		Widget(Widget* a_parent, NiNode* a_parent_node, NiTransform& a_local,
			NiPoint3 a_halfextents = NiPoint3{}, float a_radius = 1.f);
		virtual ~Widget() = default;

		void SetEnabled(bool a_enabled) { enabled = a_enabled; };
		bool IsEnabled() const { return enabled; };

		bool TestOverlap(const Hand& a_Hand) const;

		bool HandStateFilter() { return true; }
		void OnClick(bool a_activate, const Hand& a_hand, MenuAction a_action);
		void OnHover(bool a_activate, const Hand& a_hand);

		const bool IsHovered() const { return hover_state; };

		template <typename T, typename... Args>
		void AddChild(Args&&... args)
		{
			static_assert(std::is_base_of_v<Widget, T>, "T must inherit from Widget");

			children.emplace_back(std::make_unique<T>(this, std::forward<Args>(args)...));
		}

		void RemoveChild(Widget* a_child);

		Widget*                                     GetParent() const { return parent; }
		const std::vector<std::unique_ptr<Widget>>& GetChildren() const { return children; }

        // finds the base of the hierarchy tree
		const Widget* GetRoot() const;

		NiTransform& GetTransform() { return local; }
		NiTransform  GetWorld(int depth = 0) const;

		const int GetPriority() const { return priority; };

		void AddEffect(const std::string a_path);

	protected:
		NiTransform local;
		NiPoint3    extents;
		float       radius = 1.f;
		NiNode*     parent_node {};

		Widget*                              parent {};
		std::vector<std::unique_ptr<Widget>> children;
		art_addon::ArtAddonPtr               model;
		std::vector<art_addon::ArtAddonPtr>  visual_effects;
		bool                                 enabled = true;
		bool                                 hover_state = false;
		int                                  priority = 9;

		static constexpr int MAX_DEPTH = 20;
	};

	// observes and caches input device state
	class Hand
	{
	public:
		enum class State
		{
			kReady,
			kGrabbing,
			kWeapon,
			kInvalid
		};
		Hand(bool a_left) : isLeft(a_left) {}

		// Updates device state, returns true if success
		bool Update();

		const NiTransform* GetTransform() const { return transform; }
		const State        GetState() const { return state; }
		const float        GetRadius() const { return radius; }
		const NiPoint3*    GetExtents() const { return &extents; }

	private:
		bool         isLeft = false;
		NiAVObject*  node {};
		NiTransform* transform {};
		float        radius = 1.f;
		State        state = State::kReady;
		NiPoint3     extents = { 1, 1, 0.2 };
	};

	// Root widget attached to an Object Reference to allow for successive nodes to be attached
	class RootView : public Widget
	{
	public:
		RootView(TESObjectREFR* a_rootobj);
		virtual ~RootView() = default;

		TESObjectREFR* GetObjRef() const { return rootobj; }

		bool TestOverlap(Hand& a_Hand);

	protected:
		TESObjectREFR* rootobj {};
	};

	// RootView with a reference to an actor's inventory, and tracking of container state
	class Backpack : public RootView
	{
	public:
		enum class State
		{
			// backpack is dismissed/disabled, 3d is unloaded
			kClosed,
			// backpack has been summoned
			kOpen,
			// backpack is currently being grabbed by player
			kGrabbed
		};

        static constexpr FormID kNPCArmor = 1;

		Backpack(TESObjectREFR* a_rootobj, TESObjectREFR* a_owner);

		TESObjectREFR* GetOwner() const { return owner; }
		State          GetState() const { return state; }
		const Hand*    GetGrabbedBy() const { return grabbed_by; }

        void StateTransition(Backpack::State a_next_state);

		void MoveTo(const NiTransform& a_newtransform);

	private:
		// The actor that is wearing the backpack (has their inventory linked)
		TESObjectREFR* owner = nullptr;
		State          state = State::kClosed;
		Hand*          grabbed_by = nullptr;
	};

	// Individual items within a gui menu
	class ItemView : public Widget
	{
	public:
		ItemView(Widget* a_parent, NiTransform& a_transform, Item* a_item);

	protected:
		Item* item;
		int   priority = 3;
	};

	// Common interface for widgets that contain ItemViews
	class ItemContainer : public Widget
	{
	public:
		virtual void AddItem(Item* a_item_data);
		virtual void RemoveItem(Item* a_item_data);

	protected:
		int                                  priority = 3;
		std::unordered_map<Item*, ItemView*> items;
	};

	// Paginated grid of evenly spaced items
	class GridView : public ItemContainer
	{
	public:
		struct Settings
		{
			int   items_per_row = 4;
			float padding = 0.5f;
		};

		GridView(Widget* a_parent, NiTransform& a_transform);
		~GridView() {}

		void SetEnabled(bool a_enabled);

		void OnClick();
		void OnHover();

	protected:
		float CalculateItemRadius();
		int   page_index = 0;
		int   priority = 3;
	};

	// Freeform 3D container, items are associated with a transform
	// Transforms are stored by Controller
	class StorageView : public ItemContainer
	{
		struct Settings
		{
			float max_radius = 99.f;
		};

		StorageView(Widget* a_parent, NiTransform& a_transform);
		~StorageView() {}

		void SetEnabled(bool a_enabled);

		void OnClick();
		void OnHover();

	protected:
		int priority = 4;
	};

	// Container that holds a single item, animated
	class Holster : public ItemContainer
	{
	protected:
		int priority = 2;
	};

	// Implements UI interactions, highest priority
	class Button : public Widget
	{
	protected:
		int priority = 1;
	};

	// Model, contains 3D art and other data to be displayed to user
	// no state of the View objects is stored here
	class Item
	{
		RE::TESBoundObject*    base;
		int                    count = 0;
		RE::ExtraDataList*     extradata;
		art_addon::ArtAddonPtr model;
	};

	class Controller
	{
	public:
		struct ButtonEvent
		{
			bool       isLeft;
			MenuAction button;
			bool       down;
		};

		struct ActivatorOverride
		{
			Widget* owner;
			Item*   itemdata;
			bool    isLeft;
		};

		void DebugTestBackpack();

		void Update();
		void Init();
		void OnGameLoad();
		void OnBackpackEquipped();

		void MarkForDelete(Widget* w);
		void PushActivatorOverride(ActivatorOverride&& a_data);
		void RemoveActivatorOverride(Widget* a_owner);

		static Controller* GetSingleton()
		{
			static Controller singleton;
			return &singleton;
		}

		// element 0 = right hand, 1 = left hand
		std::vector<Hand> hands;

	private:
		Controller() = default;
		~Controller() = default;
		Controller(const Controller&) = delete;
		Controller(Controller&&) = delete;
		Controller& operator=(const Controller&) = delete;
		Controller& operator=(Controller&&) = delete;

		void HandleInput();
		void UpdateAnimations();
		void HandleEvents();
		void HandleDeletionQueue();
		void HandleHUDOverrides();

		void TraverseCollision(Widget& a_w, const Hand& a_hand, std::vector<Widget*>& a_hover_list);

		void ClearHUDOverride();
		void SetHUDOverride(ActivatorOverride& a_data);

		std::vector<std::unique_ptr<Backpack>> backpacks;
		std::vector<Widget*>                   widgets_to_delete;
		std::vector<ButtonEvent>               button_queue;
		std::vector<ActivatorOverride>         activator_overrides;

		std::unordered_map<Hand*, std::unordered_map<Backpack*, std::vector<Widget*>>> hovered_map;
	};

}