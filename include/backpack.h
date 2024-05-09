#pragma once
#include "NiVectorExtraData.h"
#include "art_addon.h"
#include "helper_game.h"
#include "higgsinterface001.h"
#include "main_plugin.h"

namespace backpack
{
	const std::string g_mod_name = "BackpackVR.esp";
	const std::string g_backpack_container_nodename = "Box";
	const std::string g_backpack_container_extentname = "extent";
	// ty atom
	const std::string g_rollover_nodename = "WSActivateRollover";

	class Backpack;
	class Item;
	class View;
	struct BackpackEvent;

	using NewItemID = RE::ExtraCachedScale;

	class Backpack
	{
	public:
		enum class State
		{
			/* The objectref is in the disabled worldspace.
			* Don't do anything with the backpack, it will be activated by controller events */
			kDisabled,
			/* The backpack has been summoned but the player is not interacting with it.
			* Items have been drawn and we need to check for activation on each update */
			kIdle,
			/* The player's hand is in the activation zone.
			* Need to iterate over all the items on each update
			*/
			kActive
		};

		struct MiniLayout
		{
			float width = 30;
			float height = 40;
			float item_desired_radius = 0;
			float items_per_column = 0;
			float vertical_spacing = 0;
		};

		Backpack(RE::FormID ref_id, RE::FormID wearer_ref_id) :
			ref_id(ref_id),
			wearer_ref_id(wearer_ref_id){};

		const State GetState() const { return state; }
		/* get the object reference used to implement the backpack */
		RE::TESObjectREFR* GetObjectRefr() const { return object; }
		/* get the actor that the backpack belongs to */
		const RE::TESObjectREFR* GetWearer() const { return wearer; }
		const RE::FormID         GetWearerID() const { return wearer_ref_id; }
		/* Attempts to add an item to the currently active View and the wearer's inventory */
		RE::TESBoundObject* GetDefaultBase() const { return base; }

		void Init();

		void CalculateMiniLayout();

		bool AddItem(RE::TESObjectREFR* droppedRefr) { return false; }

		void RemoveSelectedItem();
		void RemoveItem();

		void NotifyNewItemAdded();

		const bool IsValid() const
		{
			return object && object->Is3DLoaded() && wearer && wearer->Is3DLoaded() &&
				state == State::kActive;
		}

		const bool IsIgnoredItem(RE::TESBoundObject* a_obj)
		{
			return std::find(ignored_formtypes.begin(), ignored_formtypes.end(),
					   a_obj->GetFormType()) != ignored_formtypes.end();
		}
		const bool IsAllowedItem(RE::TESBoundObject* a_obj){}

	private:
		RE::TESObjectREFR*        object;
		RE::TESObjectREFR*        wearer;
		RE::FormID                ref_id;
		RE::FormID                wearer_ref_id;
		RE::TESBoundObject*       base;
		std::vector<View>         views;
		MiniLayout                mini_layout;
		State                     state;
		std::vector<RE::FormType> ignored_formtypes;
	};

	class View
	{
	public:
		View(RE::NiAVObject* root){};

		/* Adds the Item with its 3d model, returns true if there is space in the container */
		bool Add(Item& a_item) { items.push_back(a_item); }
		void Remove();
		/* Uses Alpha property to show all Items that match the filter and hide those that don't */
		void Filter(std::function<bool(const Item&)>& filter) const;

		const std::vector<Item>& GetItems() const { return items; }

	private:
		std::vector<Item> items;
		RE::NiAVObject*   root;
	};

	class MinifiedView : public View
	{};

	class NaturalView : public View
	{};

	class Holster : public View
	{};

	class Item
	{
		Item(RE::TESBoundObject* base, int count, RE::ExtraDataList* extradata,
			art_addon::ArtAddonPtr model) :
			base(base),
			count(count),
			extradata(extradata),
			model(model){};

		RE::TESBoundObject*                 base;
		int                                 count;
		RE::ExtraDataList*                  extradata;
		art_addon::ArtAddonPtr              model;
		std::vector<art_addon::ArtAddonPtr> effects;
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

	struct Animation
	{};

	class Controller
	{
	public:
		struct Settings
		{
			float shutoff_distance_player = 300;
			float shutoff_distance_npc = 100;
			int   mini_items_per_row = 5;
			float mini_horizontal_spacing = 2;
			bool  hardcore_mode = false;
			bool allow_direct_pickup = false;
		};

		static Controller* GetSingleton()
		{
			static Controller singleton;
			return &singleton;
		}

		const Settings& GetSettings() { return settings; }
		Settings&       SetSettings() { return settings; }

		Backpack* GetActiveBackpack(bool isLeft) { return active_backpack[isLeft]; }

		void PostWandUpdate();

		void OnHiggsDrop(bool isLeft, RE::TESObjectREFR* droppedRefr);
		void OnHiggsStashed(bool isLeft, RE::TESForm* stashedForm);
		void OnContainerChanged(const RE::TESContainerChangedEvent* event);

		void SendDropEvent(
			RE::TESBoundObject* a_base_object, RE::ExtraDataList* a_extradata, int a_count);

		bool IgnoreNextContainerChange();

	private:
		Controller() = default;
		~Controller() = default;
		Controller(const Controller&) = delete;
		Controller(Controller&&) = delete;
		Controller& operator=(const Controller&) = delete;
		Controller& operator=(Controller&&) = delete;

		void ProcessEvents();

		void SetActivator(Backpack* a_activation_target,
			RE::TESBoundObject* a_to_display_on_rollover, bool isLeft);
		void DisableActivator(Backpack* a_activation_target);

		void SetRolloverOverride(bool a_enable) { rollover_override = a_enable; }

		void ResetRolloverPosition();
		void OverrideRolloverPosition();

		void ResetRolloverReference(Backpack* a_target);
		void OverrideRolloverReference(
			RE::TESObjectREFR* a_target, RE::TESBoundObject* a_new_base_object);

		bool TryDropItemToHandOrGround(RE::TESBoundObject* a_base_object,
			RE::ExtraDataList* a_extradata, int a_count, bool a_hardcore);

		/* checks an inventory entry for unique identifier */
		int GetNewItemID(RE::FormID a_base_id, int a_count);

		// TODO: investigate extradata memory storage
		RE::NiTransform* GetNewItemTransform(int a_newitem_id);

		/* returns true if the transform was generated by a backpack entry event */
		bool CheckTransformSource(RE::NiTransform* a_transform);

		std::vector<Backpack>                      backpacks;
		Settings                                   settings;
		std::deque<std::unique_ptr<BackpackEvent>> event_queue;

		bool                                           rollover_override;
		Backpack*                                      active_backpack[2] = { nullptr };
		bool                                           ignore_next_container_add_event = false;
		std::unordered_map<NewItemID, RE::NiTransform*> pending_items;
	};

	inline void ClearTransformData(RE::TESObjectREFR* a_target)
	{
		if (a_target)
		{
			if (a_target->extraList.HasType(RE::ExtraDataType::kEditorRefMoveData))
			{
				a_target->extraList.RemoveByType(RE::ExtraDataType::kEditorRefMoveData);
			}
		}
	}

	inline void WriteTransformData(RE::TESObjectREFR* a_target, RE::NiTransform& a_transform)
	{
		RE::ExtraEditorRefMoveData* temp = new RE::ExtraEditorRefMoveData();
		a_transform.rotate.ToEulerAnglesXYZ(temp->realAngle);
		temp->realLocation = a_transform.translate;
		a_target->extraList.Add(temp);
	}

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