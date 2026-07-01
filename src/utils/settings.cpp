#include "settings.h"

#include "helper_game.h"
#include "rapidjson/document.h"

#include <chrono>

namespace settings
{

	void Manager::Init(std::string a_mod_name, RE::FormID a_settings_formlist)
	{
		global_vars.clear();
		mod_settings = nullptr;

		if (auto form = helper::GetForm<RE::BGSListForm>(a_settings_formlist, a_mod_name))
		{
			mod_settings = form;
		}
		else
		{
			SKSE::log::error(
				"Profile selection global variable not found ({:x})", (int)a_settings_formlist);
		}
	}

	float Manager::Get(std::string a_setting_editorid)
	{
		if (auto var = global_vars[a_setting_editorid]) { return var->value; }
		else if (mod_settings)
		{
			float result = 0.f;
			mod_settings->ForEachForm([&](RE::TESForm* f) {
				if (auto global = f->As<RE::TESGlobal>())
				{
					if (std::strcmp(global->formEditorID.c_str(), a_setting_editorid.c_str()) == 0)
					{
						global_vars[a_setting_editorid] = global;
						result = global->value;
						return RE::BSContainer::ForEachResult::kStop;
					}
				}

				return RE::BSContainer::ForEachResult::kContinue;
			});

			return result;
		}
		return -1;
	}
}
