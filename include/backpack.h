#pragma once
#include "NiVectorExtraData.h"
#include "animations.h"
#include "art_addon.h"
#include "helper_game.h"
#include "higgsinterface001.h"
#include "main_plugin.h"
#include "vrinput.h"

namespace backpack
{
	const std::string g_mod_name = "BackpackVR.esp";
	const RE::FormID  g_backpack_formID = 0xfade;
	const RE::FormID  kPlayerForm = 0x14;
	const std::string g_backpack_grab_nodename = "GrabNode";
	const std::string g_backpack_container_extentname = "extent";
	// ty atom
	const std::string g_rollover_nodename = "WSActivateRollover";

	extern RE::TESObjectREFR* g_marker_disable_objref;
	extern RE::NiPoint3       g_rollover_default_hand_pos;
	extern RE::NiMatrix3      g_rollover_default_hand_rot;
	extern float              g_default_factivatepicklength;

	class Backpack;
	class Item;
	class View;

	enum class HandState
	{
		kInvalid,
		kEmpty,
		kWeapon,
		kGrabbing
	};

	struct NewItemEvent
	{
		RE::NiTransform local;
		RE::FormID      baseObj;
		int             itemCount;
		uint16_t        ID;
		bool            isLeft;
	};

	class UIEvent
	{
	public:
		enum class Type
		{
			ViewHovered,
			CreateExtraDataEvent,
			ItemHovered
		};
		virtual Type type() = 0;
		virtual ~UIEvent() = default;
	};

	class Item
	{
	public:
		enum class State
		{
			kIdle,
			kHovered,
			kActive
		};

		Item(RE::TESBoundObject* base, int count, RE::ExtraDataList* extradata,
			art_addon::ArtAddonPtr model) :
			base(base),
			count(count),
			extradata(extradata),
			model(model){};

		State GetState(bool isLeft) const { return state[isLeft]; }
		/* Sets the state for the given hand. Also checks if a state change event should be sent */
		void SetState(bool isLeft, State a_new_state);

		float CheckOverlap(const RE::NiPoint3& a_world_pos);

		RE::TESBoundObject*                 base;
		int                                 count;
		RE::ExtraDataList*                  extradata;
		art_addon::ArtAddonPtr              model;
		std::vector<art_addon::ArtAddonPtr> effects;
		State                               state[2] = { State::kIdle };
	};

	class View
	{
	public:
		enum class State
		{
			kIdle,
			kHovered,
			kActive
		};

		enum class VisibleState
		{
			kHide,
			kShow
		};

		View(RE::NiAVObject* root) : root(root){};
		virtual ~View() = default;
		View(const View&) = delete;
		View& operator=(const View&) = delete;
		View(View&&) = default;
		View& operator=(View&&) = default;

		virtual bool CanAcceptObject(RE::TESBoundObject* a_obj) = 0;

		virtual const char* GetName() = 0;

		virtual bool InventoryAddObject(
			RE::TESObjectREFR* a_obj, RE::TESObjectREFR* a_wearer, bool isLeft);

		virtual bool IsHandle() { return false; }

		bool AddItem(const Item& a_item)
		{
			items.push_back(a_item);
			return true;
		}

		virtual bool AddItemEx(RE::TESBoundObject* a_base, RE::ExtraDataList* a_extra, int count);

		void Remove(Item* a_item)
		{
			auto it = std::find_if(
				items.begin(), items.end(), [a_item](Item& each) { return &each == a_item; });
			if (it != items.end()) { items.erase(it); }
		}

		/* Uses Alpha property to show all Items that match the filter and hide those that don't */
		virtual void Filter(std::function<bool(const RE::TESBoundObject*)> filter);

		void ToggleVisible(bool a_visible);

		Item* PickActiveItem(const RE::NiPoint3& a_world_pos, bool isLeft);

		Item* GetActiveItem(bool isLeft);

		std::vector<Item>& GetItems() { return items; }

		const State GetState(bool isLeft) const { return state[isLeft]; }

		/* Sets the state for the given hand. Also checks if a state change event should be sent */
		void SetState(bool isLeft, State a_new_state);

		RE::NiAVObject* GetRoot() { return root; }

		float CheckOverlap(const RE::NiPoint3& a_world_pos);

	protected:
		std::vector<Item> items;
		RE::NiAVObject*   root;
		State             state[2] = { State::kIdle };
		VisibleState      visibility = VisibleState::kShow;
		RE::NiPoint3      min_bound;
		RE::NiPoint3      max_bound;
	};

	class GridView : public View
	{
	public:
		static constexpr const char* kNodeName = "Grid";
		const char*                  GetName() { return kNodeName; }

		GridView(RE::NiAVObject* root) : View(root)
		{
			if (auto extents =
					root->GetExtraData<RE::NiVectorExtraData>(g_backpack_container_extentname))
			{
				min_bound = { 0, 0, 0 };
				max_bound = { extents->m_vector[0], extents->m_vector[1], extents->m_vector[2] };
			}
			SKSE::log::trace("grid created");
		}

		struct GridLayout
		{
			float width = 30;
			float height = 40;
			float item_desired_radius = 0;
			float items_per_column = 0;
			float vertical_spacing = 0;
		};
		bool CanAcceptObject(RE::TESBoundObject* a_obj);

		RE::NiPoint3 FindNextAvailableSlot(RE::TESBoundObject* a_inventory_item)
		{
			return { 0, 0, 0 };
		}

		void Filter(std::function<bool(const RE::TESBoundObject*)> filter) override;

		bool InventoryAddObject(RE::TESObjectREFR* a_obj, RE::TESObjectREFR* a_wearer, bool isLeft);

		bool AddItemEx(RE::TESBoundObject* a_base, RE::ExtraDataList* a_extra, int count) override;

		void CalculateGridLayout();

	private:
		GridLayout mini_layout;
	};

	class NormalView : public View
	{
	public:
		static constexpr const char* kNodeName = "Container";
		const char*                  GetName() { return kNodeName; }

		NormalView(RE::NiAVObject* root) : View(root)
		{
			if (auto extents =
					root->GetExtraData<RE::NiVectorExtraData>(g_backpack_container_extentname))
			{
				min_bound = { 0, 0, 0 };
				max_bound = { extents->m_vector[0], extents->m_vector[1], extents->m_vector[2] };
			}
			SKSE::log::trace("{} view created with extents: {} {} {}", GetName(), max_bound.x,
				max_bound.y, max_bound.z);
		}

		static bool Has3DPosition(RE::TESBoundObject* a_inventory_item) { return false; }

		RE::NiPoint3 FindNextAvailableSlot(RE::TESBoundObject* a_inventory_item)
		{
			return { 0, 0, 0 };
		}

		bool CanAcceptObject(RE::TESBoundObject* a_obj);
	};

	class Holster : public View
	{
	public:
		Holster(RE::NiAVObject* root) : View(root)
		{
			if (auto extents =
					root->GetExtraData<RE::NiVectorExtraData>(g_backpack_container_extentname))
			{
				min_bound = { extents->m_vector[0] / -2, extents->m_vector[1] / -2,
					extents->m_vector[2] / -2 };
				max_bound = { extents->m_vector[0] / 2, extents->m_vector[1] / 2,
					extents->m_vector[2] / 2 };
			}
			SKSE::log::trace("{} view created with extents: {} {} {} , {} {} {}", GetName(),
				min_bound.x, min_bound.y, min_bound.z, max_bound.x, max_bound.y, max_bound.z);
		}

		bool AddItemEx(RE::TESBoundObject* a_base, RE::ExtraDataList* a_extra, int count) override;

		static constexpr const char* kNodeName = "Holster";
		const char*                  GetName() { return kNodeName; }

		bool CanAcceptObject(RE::TESBoundObject* a_obj);
	};

	class Handle : public View
	{
		static constexpr float kR = 3;

	public:
		Handle(RE::NiAVObject* root) : View(root)
		{
			min_bound = { -kR, -kR, -kR };
			max_bound = { kR, kR, kR };
			SKSE::log::trace("handle created");
		}

		static constexpr const char* kNodeName = "GrabNode";
		const char*                  GetName() { return kNodeName; }

		bool IsHandle() { return true; }

		bool CanAcceptObject(RE::TESBoundObject* a_obj) { return false; }
	};

	class Backpack
	{
	public:
		enum class State
		{
			/* The objectref is in the disabled worldspace.
			* Don't do anything with the backpack, it will be activated by controller events */
			kDisabled,
			/* The backpack has been summoned but the player is not interacting with it.
			* Items have been drawn and we need to check container zones for overlap each update */
			kIdle,
			/* The player's hand is in the activation zone.
			* Need to iterate over all the items on each update
			*/
			kActive,
			/* player only, backpack is being grabbed and should be moved to hand on update */
			kGrabbed
		};

		Backpack(RE::FormID ref_id, RE::FormID wearer_ref_id) :
			ref_id(ref_id),
			wearer_ref_id(wearer_ref_id)
		{
			// Lookup and initialize references
			if (auto form = helper::GetForm(ref_id, g_mod_name))
			{
				object = form->AsReference();
				SKSE::log::trace("backpack object found: {}", ref_id);
			}
			if (auto form = helper::GetForm(wearer_ref_id, "Skyrim.esm"))
			{
				wearer = form->AsReference();
				SKSE::log::trace("backpack wearer found: {}", wearer_ref_id);
			}
			if (object)
			{
				base = object->GetObjectReference();
				object->SetActivationBlocked(true);
			}
			views.clear();
			SKSE::log::trace("backpack created for {}", wearer_ref_id);
		};
		Backpack(const Backpack&) = delete;
		Backpack& operator=(const Backpack&) = delete;
		Backpack(Backpack&& other) noexcept :
			object(other.object),
			wearer(other.wearer),
			ref_id(other.ref_id),
			wearer_ref_id(other.wearer_ref_id),
			base(other.base),
			views(std::move(other.views)),
			state(other.state)
		{
			other.object = nullptr;
			other.wearer = nullptr;
			other.base = nullptr;
		}
		Backpack& operator=(Backpack&& other) noexcept
		{
			if (this != &other)
			{
				object = other.object;
				wearer = other.wearer;
				ref_id = other.ref_id;
				wearer_ref_id = other.wearer_ref_id;
				base = other.base;
				views = std::move(other.views);
				state = other.state;

				other.object = nullptr;
				other.wearer = nullptr;
				other.base = nullptr;
			}
			return *this;
		}

		/* Initialize all the 3D data for visible backpack. Creates views and items */
		void Init();

		bool IsInit() { return !views.empty(); }

		/* Searches this backpack's Views for one that is in an Active state */
		View* GetActiveView(bool isLeft) const;

		View* GetViewByName(std::string a_name) const;

		std::vector<std::unique_ptr<View>>& GetViews() { return views; }

		/* Finds the first item which is in Active state */
		Item* GetActiveItem(bool isLeft)
		{
			if (auto v = GetActiveView(isLeft)) { return v->GetActiveItem(isLeft); }
			return nullptr;
		}

		/* Returns: true if operation was successful */
		bool InventoryAddObject(bool isLeft, RE::TESObjectREFR* a_target_ref);

		/* Attempts to drop an item into the world and return its reference */
		RE::TESObjectREFR* TryDropItem(bool isLeft, Item* a_item);

		/* Toggles the Favorite extradata and adds visual indication */
		void ToggleFavoriteItem(bool isLeft, Item* a_item);

		/* Attempts to use or equip the item, depending on the relevant settings */
		void TryEquipItem(bool isLeft, Item* a_item);

		const State GetState() const { return state; }
		void        StateTransition(State a_state);

		/* get the object reference used to implement the backpack */
		RE::TESObjectREFR* GetObjectRefr() const { return object; }

		/* get the actor that the backpack belongs to */
		RE::TESObjectREFR* GetWearer() const { return wearer; }
		const RE::FormID   GetWearerID() const { return wearer_ref_id; }

		/* Attempts to add an item to the currently active View and the wearer's inventory */
		RE::TESBoundObject* GetDefaultBase() const { return base; }

		void MoveGrabbed();

		/* Determines which view the player's hand is inside. Also sets the state of all views,
		dispatching OnViewStateChange events */
		View* PickActiveView(const RE::NiPoint3& a_world_pos, bool isLeft);

		void RemoveSelectedItem();

		void RemoveItem(Item* a_item)
		{
			for (auto& v : views) { v->Remove(a_item); }
		}

		bool CheckShutoffDistance();

		bool CheckInteractDistance();

		void NotifyNewItemAdded();

		const bool IsValid() const
		{
			return object && object->Is3DLoaded() && wearer && wearer->Is3DLoaded();
		}
		/*
		const bool IsIgnoredItem(RE::TESBoundObject* a_obj)
		{
			return std::find(ignored_formtypes.begin(), ignored_formtypes.end(),
					   a_obj->GetFormType()) != ignored_formtypes.end();
		}
		const bool IsAllowedItem(RE::TESBoundObject* a_obj) {}
*/

	private:
		RE::TESObjectREFR*                 object;
		RE::TESObjectREFR*                 wearer;
		RE::FormID                         ref_id;
		RE::FormID                         wearer_ref_id;
		RE::TESBoundObject*                base;
		std::vector<std::unique_ptr<View>> views;
		State                              state = State::kDisabled;
	};

	class Controller
	{
	public:
		enum class ItemSource
		{
			kManual,
			kGrid,
			kPickup,
			kLoot
		};

		struct Settings
		{
			float shutoff_distance_player = 400;
			float shutoff_distance_npc = 300;
			float min_interaction_distance = 150;
			float scale_big_items = 30;
			int   mini_items_per_row = 5;
			float mini_horizontal_spacing = 2;
			bool  hardcore_mode = false;
			bool  allow_equip_swapping = true;
			bool  disable_grid = false;
			bool  newitems_drop_on_loot = false;
			bool  newitems_drop_on_pickup = false;
			bool  newitems_drop_paused = false;
			bool  newitems_drop_to_ground = false;
		};

		static Controller* GetSingleton()
		{
			static Controller singleton;
			return &singleton;
		}

		void DebugSummonPlayerPack();

		void Init()
		{
			backpacks.clear();
			event_queue.clear();
			selected_backpack[0] = nullptr;
			selected_backpack[1] = nullptr;
		}

		void Add(Backpack&& a_new_backpack) { backpacks.emplace_back(std::move(a_new_backpack)); }

		const Settings& GetSettings() { return settings; }
		Settings&       SetSettings() { return settings; }

		Backpack* GetSelectedBackpack(bool isLeft) { return selected_backpack[isLeft]; }

		void PostWandUpdate();

		void OnHiggsDrop(bool isLeft, RE::TESObjectREFR* droppedRefr);
		void OnHiggsStashed(bool isLeft, RE::TESForm* stashedForm);
		void OnContainerChanged(const RE::TESContainerChangedEvent* event);
		void OnEquip(const RE::TESEquipEvent* event);

		void OnGrabAction(bool isLeft, bool start);
		void OnFavoriteAction(bool isLeft);
		void OnEquipAction(bool isLeft);

		void SetActivator(Backpack* a_activation_target,
			RE::TESBoundObject* a_to_display_on_rollover, bool isLeft);
		void DisableActivator(Backpack* a_activation_target);

		void PushNewTransform(
			RE::NiTransform a_transform, RE::FormID a_form, int a_count, uint16_t ID, bool isLeft)
		{
			pending_items.push_back({ a_transform, a_form, a_count, ID, isLeft });
		}

		RE::NiPoint3 GetHandPosition(bool isLeft);

		void SendDropEvent(
			RE::TESBoundObject* a_base_object, RE::ExtraDataList* a_extradata, int a_count);

		bool      IgnoreNextContainerChange();
		HandState GetHandState(bool isLeft);

		template <typename EventType, typename... Args>
		void PushUIEvent(Args&&... args)
		{
			event_queue.push_back(std::make_unique<EventType>(std::forward<Args>(args)...));
		}

	private:
		Controller() = default;
		~Controller() = default;
		Controller(const Controller&) = delete;
		Controller(Controller&&) = delete;
		Controller& operator=(const Controller&) = delete;
		Controller& operator=(Controller&&) = delete;

		void ProcessInput();
		void ProcessAnimations();
		void ProcessEvents();

		void SetRolloverUIPositionOverride(bool a_enable) { rollover_override = a_enable; }

		void ResetRolloverPosition();
		void OverrideRolloverPosition();

		void ResetRolloverReference(Backpack* a_target);
		void OverrideRolloverReference(
			RE::TESObjectREFR* a_target, RE::TESBoundObject* a_new_base_object);
		void MoveActivator(RE::TESObjectREFR* a_objref, bool a_enabled);

		void DropItemToHandOrGround(RE::TESBoundObject* a_base_object,
			RE::ExtraDataList* a_extradata, int a_count, bool a_hardcore);

		std::vector<Backpack>                backpacks;
		Settings                             settings;
		std::deque<std::unique_ptr<UIEvent>> event_queue;

		bool                   rollover_override;
		Backpack*              selected_backpack[2] = { nullptr };
		bool                   ignore_next_container_add_event = false;
		art_addon::ArtAddonPtr hand_effect[2];

		std::vector<NewItemEvent> pending_items;
	};

	class ViewHoverEvent : public UIEvent
	{
	public:
		ViewHoverEvent(View::State new_state, bool isLeft, View* view) :
			new_state(new_state),
			isLeft(isLeft),
			view(view)
		{}
		View::State new_state;
		bool        isLeft;
		View*       view;
		Type        type() { return Type::ViewHovered; }
	};

	class ItemHoverEvent : public UIEvent
	{
	public:
		ItemHoverEvent(Item::State new_state, Item::State old_state, bool isLeft, Item* item) :
			new_state(new_state),
			old_state(old_state),
			isLeft(isLeft),
			item(item)
		{}
		Item::State new_state;
		Item::State old_state;
		bool        isLeft;
		Item*       item;
		Type        type() { return Type::ItemHovered; }
	};

	class CreateExtraDataEvent : public UIEvent
	{
	public:
		CreateExtraDataEvent(RE::InventoryEntryData* entry, int count, RE::NiTransform local) :
			entry(entry),
			count(count),
			local(local)
		{}
		RE::InventoryEntryData* entry;
		int                     count;
		RE::NiTransform         local;
		Type                    type() { return Type::CreateExtraDataEvent; }
	};

	inline void AddObjectRefToInventory(
		RE::TESObjectREFR* a_held_object, RE::TESObjectREFR* a_new_owner)
	{
		if (a_held_object && a_new_owner && a_new_owner->As<RE::Actor>())
		{
			if (a_held_object->IsBook())
			{  // TODO: make this work :(
				a_new_owner->As<RE::Actor>()->PickUpObject(
					a_held_object, a_held_object->extraList.GetCount());
			}
			else
			{
				a_held_object->ActivateRef(
					a_new_owner, 0, 0, a_held_object->extraList.GetCount(), false);
			}
		}
	}
}