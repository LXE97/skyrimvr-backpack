#pragma once
#include "Windows.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace helper
{
	void InstallPlayerUpdateHook(std::function<void(void)> a_func);

	typedef bool (*_DrawWeapon)(RE::Actor* actor, bool draw);

	inline void PlayerDrawWeapon(bool draw)
	{
		RE::Actor* actor = RE::PlayerCharacter::GetSingleton()->As<RE::Actor>();
		uint64_t*  vtbl = *((uint64_t**)actor);
		((_DrawWeapon)(vtbl[0xA8]))(actor, draw);
	}
	void StopControllers(RE::NiAVObject* a_obj);

	RE::TESForm* LookupByName(RE::FormType a_typeEnum, const char* a_name);
	RE::FormID   GetFullFormID(uint8_t a_modindex, RE::FormID a_localID);
	uint8_t      GetFormIndex(RE::FormID a_formid);
	uint32_t     GetLocalID(RE::FormID a_formid);

	void HideActivationText(RE::TESObjectREFR* a_target, bool a_hidden);

	void CastSpellInstant(RE::Actor* a_src, RE::Actor* a_target, RE::SpellItem* sa_pell);
	void Dispel(RE::Actor* a_src, RE::Actor* a_target, RE::SpellItem* a_spell);

	float GetAVPercent(RE::Actor* a_a, RE::ActorValue a_v);
	float GetChargePercent(RE::Actor* a_a, bool isLeft);
	float GetGameHour();  //  24hr time
	float GetAmmoPercent(RE::Actor* a_a, float a_ammoCountMult);
	float GetShoutCooldownPercent(RE::Actor* a_a, float a_MaxCDTime);

	void SetGlowMult();
	void SetGlowColor(RE::NiAVObject* a_target, int a_color_hex);
	void SetSpecularMult();
	void SetSpecularColor();
	void SetTintColor();
	void SetUVCoords(RE::NiAVObject* a_target, float a_x, float a_y);

	inline RE::BSShaderProperty* GetShaderProperty(
		RE::NiAVObject* a_target, const char* a_node = nullptr)
	{
		if (a_target)
		{
			RE::NiAVObject* geo_node;
			if (a_node) { geo_node = a_target->GetObjectByName(a_node); }
			else
			{
				geo_node = a_target;
			}

			if (geo_node)
			{
				if (auto geometry = geo_node->AsGeometry())
				{
					if (auto shaderProp = geometry->GetGeometryRuntimeData().shaderProperty)
					{
						return shaderProp.get();
					}
				}
			}
		}
		return nullptr;
	}

	inline void SetWorldPosition(
		RE::NiAVObject* a_target, RE::NiAVObject* a_parent, RE::NiPoint3& a_position)
	{
		if (a_target && a_parent)
		{
			RE::NiUpdateData ctx;
			a_target->local.translate =
				a_parent->world.rotate.Transpose() * (a_position - a_parent->world.translate);
			a_target->Update(ctx);
		}
	}

	void SetUvUnique(
		RE::NiAVObject* a_target, float a_x, float a_y, const char* a_nodename = nullptr);

	void        PrintActorModelEffects(RE::TESObjectREFR* a_actor);
	void        PrintPlayerShaderEffects();
	inline void PrintVec(RE::NiPoint3& v) { SKSE::log::trace("{} {} {}", v.x, v.y, v.z); }
#define VECTOR(X) X.x, X.y, X.z

	bool InitializeSound(RE::BSSoundHandle& a_handle, std::string a_editorID);
	bool PlaySound(RE::BSSoundHandle& a_handle, float a_volume, RE::NiPoint3& a_position,
		RE::NiAVObject* a_follow_node);

	std::filesystem::path GetGamePath();
	float                 ReadFloatFromIni(std::ifstream& a_file, std::string a_setting);
	int                   ReadIntFromIni(std::ifstream& a_file, std::string a_setting);
	std::string           ReadStringFromIni(std::ifstream& a_file, std::string a_setting);
	bool                  ReadConfig(const char* a_ini_path);

	template <typename T>
	T* GetForm(const RE::FormID a_lower_id, std::string a_mod_name)
	{
		if (auto file = RE::TESDataHandler::GetSingleton()->LookupModByName(a_mod_name + ".esp"))
		{
			return RE::TESForm::LookupByID<T>(
				file->GetPartialIndex() << (file->IsLight() ? 12 : 24) | a_lower_id);
		}
		return nullptr;
	}

	RE::TESForm* GetForm(const RE::FormID a_lower_id, std::string a_mod_name);

	const char* GetObjectModelPath(RE::TESBoundObject* a_obj);
	const char* GetObjectModelPath(RE::TESObjectREFR* a_obj);

	struct GameSetting
	{
		std::string                                            friendlyName;
		std::string                                            description;
		std::uintptr_t                                         offset;
		std::variant<bool, std::int32_t, std::uint32_t, float> defaultValue;
		std::variant<bool, std::int32_t, std::uint32_t, float> minValue;
		std::variant<bool, std::int32_t, std::uint32_t, float> maxValue;
	};

	template <typename T>
	void SetGameSettingValue(
		const std::string& settingName, const GameSetting& settingData, T a_value)
	{
		if (settingData.offset == 0)
		{
			auto* setting = RE::GetINISetting(settingName.c_str());
			if (setting)
			{
				if constexpr (std::is_same_v<T, bool>)
					setting->data.b = a_value;
				else if constexpr (std::is_same_v<T, float>)
					setting->data.f = a_value;
				else if constexpr (std::is_same_v<T, std::int32_t>)
					setting->data.i = a_value;
				else if constexpr (std::is_same_v<T, std::uint32_t>)
					setting->data.u = a_value;
			}
		}
		else
		{
			auto address = REL::Offset{ settingData.offset }.address();
			*reinterpret_cast<T*>(address) = a_value;
		}
	}

	void GetVertices(RE::NiAVObject* a_root, std::vector<RE::NiPoint3>& a_vertices);

	void CalculateSphereBounds(
		const std::vector<RE::NiPoint3>& a_vertices, float& a_radiusOut, RE::NiPoint3& a_centerOut);

	void CalculateAABB(const std::vector<RE::NiPoint3>& a_vertices, RE::NiPoint3& a_center,
		RE::NiPoint3& a_extents);

	void CalculateBoundsDirect(RE::NiAVObject* a_root, float& a_radiusOut,
		RE::NiPoint3& a_centerOut, RE::NiPoint3& a_extentsOut);
}
