#include "helper_game.h"

#include "helper_math.h"

namespace helper
{
	using namespace RE;

	TESForm* LookupByName(FormType a_typeEnum, const char* a_name)
	{
		auto* data = TESDataHandler::GetSingleton();
		auto& forms = data->GetFormArray(a_typeEnum);
		for (auto*& form : forms)
		{
			if (!strcmp(form->GetName(), a_name)) { return form; }
		}
		return nullptr;
	}

	RE::FormID GetFullFormID(uint8_t a_modindex, RE::FormID a_localID)
	{
		return (a_modindex << 24) | a_localID;
	}

	void HideActivationText(TESObjectREFR* a_target, bool a_hidden)
	{
		a_target->extraList.SetExtraFlags(ExtraFlags::Flag::kBlockActivateText, a_hidden);
	}

	float GetAVPercent(Actor* a_a, ActorValue a_v)
	{
		float current = a_a->AsActorValueOwner()->GetActorValue(a_v);
		float base = a_a->AsActorValueOwner()->GetBaseActorValue(a_v);
		float mod = a_a->GetActorValueModifier(ACTOR_VALUE_MODIFIER::kPermanent, a_v) +
			a_a->GetActorValueModifier(ACTOR_VALUE_MODIFIER::kTemporary, a_v);
		return current / (base + mod);
	}

	float GetChargePercent(Actor* a_a, bool isLeft)
	{
		if (auto equipped = a_a->GetEquippedObject(isLeft))
		{
			if (equipped->IsWeapon())
			{
				float current = a_a->AsActorValueOwner()->GetActorValue(
					isLeft ? ActorValue::kLeftItemCharge : ActorValue::kRightItemCharge);

				// player made items
				if (auto entryData = a_a->GetEquippedEntryData(isLeft))
				{
					for (auto& x : *(entryData->extraLists))
					{
						if (auto e = x->GetByType<ExtraEnchantment>())
						{
							return std::clamp(current / e->charge, 0.f, 100.f);
						}
					}
				}

				// prefab items
				if (auto ench = equipped->As<TESEnchantableForm>())
				{
					return std::clamp(current / (float)(ench->amountofEnchantment), 0.f, 100.f);
				}
			}
		}
		return 0;
	}

	float GetGameHour()
	{
		if (auto c = Calendar::GetSingleton()) { return c->GetHour(); }
		return 0;
	}

	float GetAmmoPercent(Actor* a_a, float a_ammoCountMult)
	{
		if (auto ammo = a_a->GetCurrentAmmo())
		{
			auto countmap = a_a->GetInventoryCounts();
			if (countmap[ammo]) { return std::clamp(countmap[ammo] * a_ammoCountMult, 0.f, 100.f); }
		}
		return 0;
	}

	float GetShoutCooldownPercent(Actor* a_a, float a_MaxCDTime)
	{
		return std::clamp(a_a->GetVoiceRecoveryTime() / a_MaxCDTime, 0.f, 100.f);
	}

	void SetGlowMult() {}

	void SetGlowColor(NiAVObject* a_target, int a_color_hex)
	{
		if (a_target)
		{
			// if target has no geometry, try first child
			BSGeometry* geom = a_target->AsGeometry();
			if (!geom) { geom = a_target->AsNode()->GetChildren().front()->AsGeometry(); }
			if (auto shaderProp = GetShaderProperty(geom))
			{
				if (auto shader = netimmerse_cast<RE::BSLightingShaderProperty*>(shaderProp))
				{
					NiColor temp(a_color_hex);
					*(shader->emissiveColor) = temp;
				}
			}
		}
	}

	void SetUVCoords(NiAVObject* a_target, float a_x, float a_y)
	{
		if (a_target)
		{
			if (auto geometry =
					a_target->GetFirstGeometryOfShaderType(BSShaderMaterial::Feature::kNone))
			{
				if (auto shaderProp = geometry->GetGeometryRuntimeData()
										  .properties[RE::BSGeometry::States::kEffect]
										  .get())
				{
					if (auto shader = netimmerse_cast<RE::BSShaderProperty*>(shaderProp))
					{
						shader->material->texCoordOffset[0].x = a_x;
						shader->material->texCoordOffset[0].y = a_y;
						shader->material->texCoordOffset[1].x = a_x;
						shader->material->texCoordOffset[1].y = a_y;
					}
				}
			}
		}
	}

	void SetSpecularMult() {}
	void SetSpecularColor() {}

	void CastSpellInstant(Actor* src, Actor* a_target, SpellItem* a_spell)
	{
		if (src && a_target && a_spell)
		{
			if (auto caster = src->GetMagicCaster(MagicSystem::CastingSource::kInstant))
			{
				caster->CastSpellImmediate(a_spell, false, a_target, 1.0, false, 1.0, src);
			}
		}
	}

	void Dispel(Actor* src, Actor* a_target, SpellItem* a_spell)
	{
		if (src && a_target && a_spell)
		{
			if (auto handle = src->GetHandle())
			{
				a_target->GetMagicTarget()->DispelEffect(a_spell, handle);
			}
		}
	}

	void PrintActorModelEffects(RE::TESObjectREFR* a_actor)
	{
		if (const auto processLists = RE::ProcessLists::GetSingleton())
		{
			int player = 0;
			int dangling = 0;
			processLists->ForEachModelEffect([&](RE::ModelReferenceEffect& a_modelEffect) {
				if (a_actor == nullptr || a_modelEffect.target.get()->AsReference() == a_actor)
				{
					if (a_modelEffect.artObject)
					{
						SKSE::log::debug(
							"MRE:{}  AO:{}", (void*)&a_modelEffect, (void*)a_modelEffect.artObject);
						player++;
					}
					else { dangling++; }
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});
			SKSE::log::debug("{} effects and {} dangling MRE", player, dangling);
		}
	}

	void PrintPlayerShaderEffects()
	{
		if (const auto processLists = RE::ProcessLists::GetSingleton())
		{
			int player = 0;
			int dangling = 0;
			processLists->ForEachShaderEffect([&](RE::ShaderReferenceEffect& a_shaderEffect) {
				if (a_shaderEffect.target.get()->AsReference() ==
					RE::PlayerCharacter::GetSingleton()->AsReference())
				{
					if (a_shaderEffect.effectData)
					{
						SKSE::log::debug("SRE:{}  AO:{}", (void*)&a_shaderEffect,
							(void*)a_shaderEffect.effectData);
						player++;
					}
					else { dangling++; }
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});
			SKSE::log::debug("{} player effects and {} dangling SRE", player, dangling);
		}
	}

	std::filesystem::path GetGamePath()
	{
		HMODULE handle = GetModuleHandle(NULL);
		char    exe_path[MAX_PATH];
		// Get the full path of the DLL
		if (GetModuleFileNameA(handle, exe_path, (sizeof(exe_path))))
		{
			std::filesystem::path file_path(exe_path);

			return file_path.parent_path() / "Data\\";
		}
		return "";
	}

	float ReadFloatFromIni(std::ifstream& a_file, std::string a_setting)
	{
		if (a_file.is_open())
		{
			std::string line;
			while (std::getline(a_file, line))
			{
				if (line[0] != '#' && line.find(a_setting) == 0)
				{
					auto found = line.find('=');
					if (found != std::string::npos)
					{
						a_file.clear();
						a_file.seekg(0, a_file.beg);
						auto val = std::stof(line.substr(found + 1));
						SKSE::log::trace("{} : {}", a_setting, val);
						return val;
					}
				}
			}
		}

		return 0.f;
	}

	int ReadIntFromIni(std::ifstream& a_file, std::string a_setting)
	{
		if (a_file.is_open())
		{
			std::string line;
			while (std::getline(a_file, line))
			{
				if (line.find(a_setting) == 0)
				{
					auto found = line.find('=');
					if (found != std::string::npos)
					{
						a_file.clear();
						a_file.seekg(0, std::ios::beg);
						auto val = std::stoi(line.substr(found + 1));
						SKSE::log::trace("{} : {}", a_setting, val);
						return val;
					}
				}
			}
		}

		return 0;
	}

	std::string ReadStringFromIni(std::ifstream& a_file, std::string a_setting)
	{
		if (a_file.is_open())
		{
			std::string line;
			while (std::getline(a_file, line))
			{
				if (line.find(a_setting) == 0)
				{
					auto found = line.find('=');
					if (found != std::string::npos)
					{
						a_file.clear();
						a_file.seekg(0, std::ios::beg);

						// Extract the substring after '=' and trim any leading/trailing whitespace
						std::string val = line.substr(found + 1);
						val = val.erase(
							0, val.find_first_not_of(" \t\n\r"));  // Trim leading whitespace
						val = val.erase(
							val.find_last_not_of(" \t\n\r") + 1);  // Trim trailing whitespace

						SKSE::log::trace("{} : {}", a_setting, val);
						return val;
					}
				}
			}
		}

		return "";
	}

	bool InitializeSound(BSSoundHandle& a_handle, std::string a_editorID)
	{
		auto man = BSAudioManager::GetSingleton();
		man->BuildSoundDataFromEditorID(a_handle, a_editorID.c_str(), 0x10);
		return a_handle.IsValid();
	}

	bool PlaySound(BSSoundHandle& a_handle, float a_volume, RE::NiPoint3& a_position,
		RE::NiAVObject* a_follow_node)
	{
		a_handle.SetPosition(a_position);
		a_handle.SetObjectToFollow(a_follow_node);
		a_handle.SetVolume(a_volume);
		a_handle.Play();
		return a_handle.IsPlaying();
	}

	RE::TESForm* GetForm(const RE::FormID a_lower_id, std::string a_mod_name)
	{
		if (auto file = RE::TESDataHandler::GetSingleton()->LookupModByName(a_mod_name))
		{
			return RE::TESForm::LookupByID(
				file->GetPartialIndex() << (file->IsLight() ? 12 : 24) | a_lower_id);
		}
		return nullptr;
	}

	const char* GetObjectModelPath(RE::TESBoundObject* a_obj)
	{
		if (auto model = a_obj->As<TESModelTextureSwap>()) { return model->GetModel(); }
		else if (a_obj->GetFormType() == FormType::Armor)
		{
			if (auto bipedmodel = a_obj->As<TESBipedModelForm>())
			{
				return bipedmodel->worldModels[0].GetModel();
			}
		}
		return nullptr;
	}

	const char* GetObjectModelPath(RE::TESObjectREFR* a_obj)
	{
		if (auto boundobj = a_obj->GetBaseObject())
		{
			if (auto model = boundobj->As<TESModelTextureSwap>()) { return model->GetModel(); }
			else if (boundobj->GetFormType() == FormType::Armor)
			{
				if (auto bipedmodel = boundobj->As<TESBipedModelForm>())
				{
					return bipedmodel->worldModels[0].GetModel();
				}
			}
		}
		return nullptr;
	}

}