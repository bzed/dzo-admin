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
		dependencies[] = {"Game", "World", "Mission"};
		// Lets other mods use the marker API behind #ifdef DZO_ADMIN.
		defines[] = {"DZO_ADMIN"};

		class defs
		{
			class gameScriptModule
			{
				value = "";
				files[] = {"DZOAdmin/scripts/3_Game"};
			};
			class worldScriptModule
			{
				value = "";
				files[] = {"DZOAdmin/scripts/4_World"};
			};
			class missionScriptModule
			{
				value = "";
				files[] = {"DZOAdmin/scripts/5_Mission"};
			};
		};
	};
};
