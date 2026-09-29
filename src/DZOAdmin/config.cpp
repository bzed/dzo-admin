// SPDX-FileCopyrightText: 2026 Bernd Zeimetz <bernd@bzed.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

class CfgPatches
{
	class DZOAdmin
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] = {"DZ_Data", "DZ_Scripts"};
	};
};

class CfgMods
{
	class DZOAdmin
	{
		dir = "DZOAdmin";
		name = "DZOAdmin";
		author = "Bernd Zeimetz";
		type = "servermod";
		dependencies[] = {"Game"};

		class defs
		{
			class gameScriptModule
			{
				value = "";
				files[] = {"DZOAdmin/scripts/3_Game"};
			};
		};
	};
};
