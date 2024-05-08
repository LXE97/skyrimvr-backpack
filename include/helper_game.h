#pragma once
#include "Windows.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace helper
{
	using namespace RE;
	RE::TESForm* LookupByName(RE::FormType a_typeEnum, const char* a_name);
	RE::FormID   GetFullFormID(uint8_t a_modindex, RE::FormID a_localID);

	void HideActivationText(TESObjectREFR *a_target, bool a_hidden);

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
			else { geo_node = a_target; }

			if (geo_node)
			{
				if (auto geometry = geo_node->AsGeometry())
				{
					if (auto property = geometry->properties[RE::BSGeometry::States::kEffect].get())
					{
						if (auto shader = netimmerse_cast<RE::BSShaderProperty*>(property))
						{
							return shader;
						}
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
			NiUpdateData ctx;
			a_target->local.translate =
				a_parent->world.rotate.Transpose() * (a_position - a_parent->world.translate);
			a_target->Update(ctx);
		}
	}

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

	RE::TESForm* GetForm(const RE::FormID a_lower_id, std::string a_mod_name);

	const char* GetObjectModelPath(const RE::TESBoundObject& a_obj);
	const char* GetObjectModelPath(const RE::TESObjectREFR* a_obj);

}
