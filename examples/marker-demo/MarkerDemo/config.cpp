// SPDX-FileCopyrightText: 2026 Bernd Zeimetz <bernd@bzed.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

class CfgPatches
{
	class MarkerDemo
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		// No DZOAdmin here: the demo must load without it.
		requiredAddons[] = {"DZ_Data", "DZ_Scripts"};
	};
};

class CfgMods
{
	class MarkerDemo
	{
		dir = "MarkerDemo";
		name = "MarkerDemo";
		author = "Bernd Zeimetz";
		type = "servermod";
		dependencies[] = {"Mission"};

		class defs
		{
			class missionScriptModule
			{
				value = "";
				files[] = {"MarkerDemo/scripts/5_Mission"};
			};
		};
	};
};
