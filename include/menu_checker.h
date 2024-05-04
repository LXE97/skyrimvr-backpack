// Shizof's method
#pragma once

#include <unordered_map>

namespace menuchecker
{
	extern std::vector<std::string> gameStoppingMenus;

	extern std::unordered_map<std::string, bool> menuTypes;

	bool isGameStopped();

	void begin();

	void onMenuOpenClose(RE::MenuOpenCloseEvent const* evn);
}
