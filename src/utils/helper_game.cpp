#include "helper_game.h"

#include "helper_math.h"

namespace helper
{
	struct PlayerCharacter_Update
	{
		static void thunk(RE::PlayerCharacter* a_player, float a_delta)
		{
			func(a_player, a_delta);
			if (my_func) { my_func(); }
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static inline std::function<void(void)>        my_func;
	};

	void InstallPlayerUpdateHook(std::function<void(void)> a_func)
	{
		PlayerCharacter_Update::my_func = a_func;
		RE::write_vfunc<RE::PlayerCharacter, 0xAF, PlayerCharacter_Update>();
	}

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
	{ return (a_modindex << 24) | a_localID; }

	uint8_t GetFormIndex(RE::FormID a_formid) { return a_formid >> 24; }

	uint32_t GetLocalID(RE::FormID a_formid) { return a_formid & 0x00FFFFFF; }

	void HideActivationText(TESObjectREFR* a_target, bool a_hidden)
	{ a_target->extraList.SetExtraFlags(ExtraFlags::Flag::kBlockActivateText, a_hidden); }

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
	{ return std::clamp(a_a->GetVoiceRecoveryTime() / a_MaxCDTime, 0.f, 100.f); }

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
				if (auto shaderProp = geometry->GetGeometryRuntimeData().shaderProperty)
				{
					shaderProp->material->texCoordOffset[0].x = a_x;
					shaderProp->material->texCoordOffset[0].y = a_y;
					shaderProp->material->texCoordOffset[1].x = a_x;
					shaderProp->material->texCoordOffset[1].y = a_y;
				}
			}
		}
	}

	void SetUvUnique(NiAVObject* a_target, float a_x, float a_y, const char* a_nodename)
	{
		if (auto shader = helper::GetShaderProperty(a_target, a_nodename))
		{
			auto oldmat = shader->material;
			auto newmat = oldmat->Create();
			newmat->CopyMembers(oldmat);
			shader->material = newmat;
			newmat->IncRef();
			oldmat->DecRef();

			newmat->texCoordOffset[0].x = a_x;
			newmat->texCoordOffset[0].y = a_y;
			newmat->texCoordOffset[1].x = newmat->texCoordOffset[0].x;
			newmat->texCoordOffset[1].y = newmat->texCoordOffset[0].y;
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
			processLists->ForEachModelEffect([&](RE::ModelReferenceEffect* a_modelEffect) {
				if (a_actor == nullptr || a_modelEffect->target.get()->AsReference() == a_actor)
				{
					if (a_modelEffect->artObject)
					{
						SKSE::log::debug("MRE:{}  AO:{:x}", (void*)&a_modelEffect,
							a_modelEffect->artObject->GetFormID());
						player++;
					}
					else
					{
						SKSE::log::debug("MRE:{}  AO:{}", (void*)&a_modelEffect,
							(void*)(a_modelEffect->artObject));
						dangling++;
					}
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
			processLists->ForEachShaderEffect([&](RE::ShaderReferenceEffect* a_shaderEffect) {
				if (a_shaderEffect->target.get()->AsReference() ==
					RE::PlayerCharacter::GetSingleton()->AsReference())
				{
					if (a_shaderEffect->effectData)
					{
						SKSE::log::debug("SRE:{}  AO:{}", (void*)&a_shaderEffect,
							(void*)a_shaderEffect->effectData);
						player++;
					}
					else
					{
						dangling++;
					}
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
						try
						{
							auto val = std::stof(line.substr(found + 1));
							SKSE::log::trace("{} : {}", a_setting, val);
							return val;
						} catch (std::out_of_range)
						{
						} catch (std::invalid_argument) {}
						SKSE::log::error("Bad mod ini, please reset it");
					}
				}
			}
			SKSE::log::error("ini error: {} not found", a_setting);
			a_file.clear();
			a_file.seekg(0, a_file.beg);
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
						try
						{
							auto val = std::stoi(line.substr(found + 1));
							SKSE::log::trace("{} : {}", a_setting, val);
							return val;
						} catch (std::out_of_range)
						{
						} catch (std::invalid_argument) {}
						SKSE::log::error("Bad mod ini, please reset it");
					}
				}
			}
			SKSE::log::error("ini error: {} not found", a_setting);
			a_file.clear();
			a_file.seekg(0, std::ios::beg);
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
			SKSE::log::error("ini error: {} not found", a_setting);
			a_file.clear();
			a_file.seekg(0, std::ios::beg);
		}

		return "";
	}

	// TODO: figure out new sound API
	// bool InitializeSound(BSSoundHandle& a_handle, std::string a_editorID)
	// {
	// 	auto man = BSAudioManager::GetSingleton();
	// 	man->BuildSoundDataFromEditorID(a_handle, a_editorID.c_str(), 0x10);
	// 	return a_handle.IsValid();
	// }

	void StopControllers(RE::NiAVObject* a_obj)
	{
		if (!a_obj) { return; }

		for (auto* controller = a_obj->GetControllers(); controller;
			controller = controller->next.get())
		{
			controller->Stop();
		}

		if (auto* node = a_obj->AsNode())
		{
			for (auto& child : node->children) { StopControllers(child.get()); }
		}
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

	const char* GetObjectModelPath(RE::TESBoundObject* a_obj)
	{
		if (!a_obj) { return nullptr; }

		switch (a_obj->GetFormType())
		{
		case RE::FormType::Armor:
			if (auto armor = a_obj->As<RE::TESObjectARMO>())
			{
				if (auto path = armor->worldModels[0].GetModel(); path && path[0] != '\0')
				{
					return path;
				}
				if (auto path = armor->worldModels[1].GetModel(); path && path[0] != '\0')
				{
					return path;
				}
			}
			break;

		case RE::FormType::Book:
			if (auto book = a_obj->As<RE::TESObjectBOOK>())
			{
				if (auto model = book->inventoryModel)
				{
					if (auto data = model->As<RE::TESModelTextureSwap>())
					{
						return data->GetModel();
					}
				}
			}
			break;

		case RE::FormType::Ammo:
		case RE::FormType::KeyMaster:
		case RE::FormType::Weapon:
		case RE::FormType::AlchemyItem:
		case RE::FormType::SoulGem:
		case RE::FormType::Misc:
		case RE::FormType::Ingredient:
		case RE::FormType::Scroll:
		default:
			if (auto model = a_obj->As<RE::TESModelTextureSwap>())
			{
				if (auto path = model->GetModel(); path && path[0] != '\0') { return path; }
			}
		}

		SKSE::log::trace("No model found for formID {} with formtype {}", a_obj->GetFormID(),
			RE::FormTypeToString(a_obj->GetFormType()));
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

	RE::TESForm* GetForm(const RE::FormID a_lower_id, std::string a_mod_name)
	{
		if (auto file = RE::TESDataHandler::GetSingleton()->LookupModByName(a_mod_name))
		{
			if (auto idx = file->GetPartialIndex(); idx != 0xff)
			{
				return RE::TESForm::LookupByID(idx << (file->IsLight() ? 12 : 24) | a_lower_id);
			}
		}
		return nullptr;
	}

	/* Code based on https://github.com/adamhynek/higgs/ */

	void UpdateVertices(RE::BSGeometry* a_geom, const RE::NiTransform& a_geometryToRoot,
		std::vector<RE::NiPoint3>& a_vertices)
	{
		if (!a_geom) { return; }

		auto* trishape = a_geom->AsTriShape();
		if (!trishape) { return; }

		const auto vertex_count = trishape->GetTrishapeRuntimeData().vertexCount;

		auto* geom_data = a_geom->GetGeometryRuntimeData().rendererData;
		if (!geom_data || !geom_data->rawVertexData || vertex_count == 0) { return; }

		auto vertex_desc = geom_data->vertexDesc;

		if (!vertex_desc.HasFlag(RE::BSGraphics::Vertex::Flags::VF_VERTEX)) { return; }

		auto*      verts = geom_data->rawVertexData;
		const auto vertex_size = vertex_desc.GetSize();
		const auto pos_offset =
			vertex_desc.GetAttributeOffset(RE::BSGraphics::Vertex::Attribute::VA_POSITION);

		for (std::uint16_t i = 0; i < vertex_count; ++i)
		{
			auto* pos = reinterpret_cast<RE::NiPoint3*>(verts + i * vertex_size + pos_offset);

			a_vertices.push_back(a_geometryToRoot * (*pos));
		}
	}

	void UpdateVertices(RE::NiSkinInstance* a_skin, const RE::NiTransform& a_geometryToRoot,
		std::vector<RE::NiPoint3>& a_vertices)
	{
		if (!a_skin) { return; }

		auto& part = a_skin->skinPartition;
		if (!part) { return; }

		for (const auto& data : part->partitions)
		{
			const auto vertex_count = data.vertices;

			auto* geom_data = data.buffData;
			if (!geom_data || !geom_data->rawVertexData || vertex_count == 0) { continue; }

			auto vertex_desc = geom_data->vertexDesc;

			if (!vertex_desc.HasFlag(RE::BSGraphics::Vertex::Flags::VF_VERTEX)) { continue; }

			auto*      verts = geom_data->rawVertexData;
			const auto vertex_size = vertex_desc.GetSize();
			const auto pos_offset =
				vertex_desc.GetAttributeOffset(RE::BSGraphics::Vertex::Attribute::VA_POSITION);

			for (std::uint16_t i = 0; i < vertex_count; ++i)
			{
				auto* pos = reinterpret_cast<RE::NiPoint3*>(verts + i * vertex_size + pos_offset);

				a_vertices.push_back(a_geometryToRoot * (*pos));
			}
		}
	}

	void GetVerticesRecursive(RE::NiAVObject* a_obj, const RE::NiTransform& a_parentToRoot,
		std::vector<RE::NiPoint3>& a_vertices)
	{
		if (!a_obj) { return; }

		const RE::NiTransform object_to_root = a_parentToRoot * a_obj->local;

		if (auto* geom = a_obj->AsGeometry())
		{
			if (auto* skin = geom->skinInstance.get())
			{
				UpdateVertices(skin, object_to_root, a_vertices);
				return;
			}
			else
			{
				UpdateVertices(geom, object_to_root, a_vertices);
				return;
			}
		}

		if (auto* node = a_obj->AsNode())
		{
			for (auto& child : node->children)
			{
				GetVerticesRecursive(child.get(), object_to_root, a_vertices);
			}
		}
	}

	void GetVertices(RE::NiAVObject* a_root, std::vector<RE::NiPoint3>& a_vertices)
	{
		if (!a_root) { return; }
		if (auto node = a_root->AsNode())
		{
			for (auto& c : node->children) { GetVerticesRecursive(c.get(), {}, a_vertices); }
		}
	}

	void CalculateSphereBounds(
		const std::vector<RE::NiPoint3>& a_vertices, float& a_radiusOut, RE::NiPoint3& a_centerOut)
	{
		a_radiusOut = 0.0f;
		a_centerOut = { 0.0f, 0.0f, 0.0f };

		if (a_vertices.empty()) { return; }

		RE::NiPoint3 min = a_vertices.front();
		RE::NiPoint3 max = a_vertices.front();

		for (const auto& v : a_vertices)
		{
			min.x = std::min(min.x, v.x);
			min.y = std::min(min.y, v.y);
			min.z = std::min(min.z, v.z);

			max.x = std::max(max.x, v.x);
			max.y = std::max(max.y, v.y);
			max.z = std::max(max.z, v.z);
		}

		a_centerOut = { (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f };

		for (const auto& v : a_vertices)
		{
			const auto  delta = v - a_centerOut;
			const float distSq = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
			a_radiusOut = std::max(a_radiusOut, std::sqrt(distSq));
		}
	}

	void CalculateAABB(const std::vector<RE::NiPoint3>& a_vertices, RE::NiPoint3& a_center,
		RE::NiPoint3& a_extents)
	{
		if (a_vertices.empty())
		{
			a_center = {};
			a_extents = {};
			return;
		}

		auto a_min = a_vertices.front();
		auto a_max = a_vertices.front();

		for (const auto& v : a_vertices)
		{
			a_min.x = std::min(a_min.x, v.x);
			a_min.y = std::min(a_min.y, v.y);
			a_min.z = std::min(a_min.z, v.z);

			a_max.x = std::max(a_max.x, v.x);
			a_max.y = std::max(a_max.y, v.y);
			a_max.z = std::max(a_max.z, v.z);
		}

		a_center = (a_min + a_max) * 0.5f;
		a_extents = (a_max - a_min) * 0.5f;
	}

	void CalculateExtentsAndRadius(const std::vector<RE::NiPoint3>& a_vertices,
		RE::NiPoint3& a_center, RE::NiPoint3& a_extents, float& a_radius)
	{
		if (a_vertices.empty()) return;
	}

	struct BoundsAccumulator
	{
		bool         initialized = false;
		RE::NiPoint3 min{};
		RE::NiPoint3 max{};

		void Add(const RE::NiPoint3& a_point)
		{
			if (!initialized)
			{
				min = a_point;
				max = a_point;
				initialized = true;
				return;
			}

			min.x = std::min(min.x, a_point.x);
			min.y = std::min(min.y, a_point.y);
			min.z = std::min(min.z, a_point.z);

			max.x = std::max(max.x, a_point.x);
			max.y = std::max(max.y, a_point.y);
			max.z = std::max(max.z, a_point.z);
		}
	};

	void UpdateBoundsDirect(RE::BSGeometry* a_geom, const RE::NiTransform& a_geometryToRoot,
		BoundsAccumulator& a_bounds)
	{
		if (!a_geom) { return; }

		auto* trishape = a_geom->AsTriShape();
		if (!trishape) { return; }

		const auto vertex_count = trishape->GetTrishapeRuntimeData().vertexCount;

		auto* geom_data = a_geom->GetGeometryRuntimeData().rendererData;
		if (!geom_data || !geom_data->rawVertexData || vertex_count == 0) { return; }

		auto vertex_desc = geom_data->vertexDesc;

		if (!vertex_desc.HasFlag(RE::BSGraphics::Vertex::Flags::VF_VERTEX)) { return; }

		auto*      verts = geom_data->rawVertexData;
		const auto vertex_size = vertex_desc.GetSize();

		const auto pos_offset =
			vertex_desc.GetAttributeOffset(RE::BSGraphics::Vertex::Attribute::VA_POSITION);

		for (std::uint16_t i = 0; i < vertex_count; ++i)
		{
			auto* pos = reinterpret_cast<RE::NiPoint3*>(verts + i * vertex_size + pos_offset);

			a_bounds.Add(a_geometryToRoot * (*pos));
		}
	}

	void UpdateBoundsDirect(RE::NiSkinInstance* a_skin, const RE::NiTransform& a_geometryToRoot,
		BoundsAccumulator& a_bounds)
	{
		if (!a_skin) { return; }

		auto& part = a_skin->skinPartition;
		if (!part) { return; }

		for (const auto& data : part->partitions)
		{
			const auto vertex_count = data.vertices;

			auto* geom_data = data.buffData;
			if (!geom_data || !geom_data->rawVertexData || vertex_count == 0) { continue; }

			auto vertex_desc = geom_data->vertexDesc;

			if (!vertex_desc.HasFlag(RE::BSGraphics::Vertex::Flags::VF_VERTEX)) { continue; }

			auto*      verts = geom_data->rawVertexData;
			const auto vertex_size = vertex_desc.GetSize();

			const auto pos_offset =
				vertex_desc.GetAttributeOffset(RE::BSGraphics::Vertex::Attribute::VA_POSITION);

			for (std::uint16_t i = 0; i < vertex_count; ++i)
			{
				auto* pos = reinterpret_cast<RE::NiPoint3*>(verts + i * vertex_size + pos_offset);

				a_bounds.Add(a_geometryToRoot * (*pos));
			}
		}
	}

	void CalculateBoundsDirectRecursive(
		RE::NiAVObject* a_obj, const RE::NiTransform& a_parentToRoot, BoundsAccumulator& a_bounds)
	{
		if (!a_obj) { return; }

		const RE::NiTransform object_to_root = a_parentToRoot * a_obj->local;

		if (auto* geom = a_obj->AsGeometry())
		{
			if (auto* skin = geom->skinInstance.get())
			{
				UpdateBoundsDirect(skin, object_to_root, a_bounds);
			}
			else
			{
				UpdateBoundsDirect(geom, object_to_root, a_bounds);
			}

			return;
		}

		if (auto* node = a_obj->AsNode())
		{
			for (auto& child : node->children)
			{
				CalculateBoundsDirectRecursive(child.get(), object_to_root, a_bounds);
			}
		}
	}

	void CalculateBoundsDirect(RE::NiAVObject* a_root, float& a_radiusOut,
		RE::NiPoint3& a_centerOut, RE::NiPoint3& a_extentsOut)
	{
		a_radiusOut = 0.f;
		a_centerOut = {};
		a_extentsOut = {};

		if (!a_root) { return; }

		auto* root_node = a_root->AsNode();
		if (!root_node) { return; }

		BoundsAccumulator bounds;

		const RE::NiTransform root_identity{};

		for (auto& child : root_node->children)
		{
			CalculateBoundsDirectRecursive(child.get(), root_identity, bounds);
		}

		if (!bounds.initialized) { return; }

		a_centerOut = (bounds.min + bounds.max) * 0.5f;
		a_extentsOut = (bounds.max - bounds.min) * 0.5f;
		a_radiusOut = a_extentsOut.Length();
	}

}