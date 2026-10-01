// SPDX-FileCopyrightText: 2026 Bernd Zeimetz <bernd@bzed.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

modded class MissionServer
{
	override void OnInit()
	{
		super.OnInit();
#ifdef DZO_ADMIN
		DZOAdmin_Map.DefineLayer("demo", "zone", "#e8a33d");
		// A static point with a tooltip, expiring after an hour.
		map<string, string> props = new map<string, string>;
		props.Set("tier", "3");
		DZOAdmin_Map.Upsert("demo", "start", "7500 0 7500", "zone", "Demo point", props, 3600);
		// A zone.
		DZOAdmin_Map.Circle("demo", "zone1", "7600 0 7500", 150, "Demo zone");
#endif
	}
}
