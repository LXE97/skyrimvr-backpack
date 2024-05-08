#pragma once
#include "NiVectorExtraData.h"
#include "art_addon.h"
#include "helper_game.h"

namespace backpack
{
	using namespace RE;
	const std::string  g_mod_name = "BackpackVR.esp";
	extern std::string g_backpack_container_nodename;
	extern std::string g_backpack_container_extentname;
	extern int         g_mini_items_per_row;
	extern float       g_mini_horizontal_spacing;

	struct BackpackItemView
	{
		enum class Type
		{
			kNatural,
			kMinified
		};

		BackpackItemView(Type type, TESBoundObject* base, int count, ExtraDataList* extradata,
			art_addon::ArtAddonPtr model) :
			type(type),
			base(base),
			count(count),
			extradata(extradata),
			model(model){};

		Type                   type;
		TESBoundObject*        base;
		int                    count;
		ExtraDataList*         extradata;
		art_addon::ArtAddonPtr model;
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

	class Backpack
	{
	public:
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

		void Init();

		void CalculateMiniLayout();

		TESObjectREFR*                object;
		TESObjectREFR*                wearer;
		RE::FormID                    ref_id;
		RE::FormID                    wearer_ref_id;
		TESBoundObject*               base;
		std::vector<BackpackItemView> item_view;
		MiniLayout                    mini_layout;
	};
}