// SPDX-FileCopyrightText: 2026 Bernd Zeimetz <bernd@bzed.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

// Who drove a vehicle last. Taking the driver's seat counts as using the vehicle; passengers do not.
// The memory is the mod's own and in RAM only: nothing is stored on the vehicles, so their persistence
// is unchanged. It starts empty after a server restart (dzo keeps the record in its database).
class DZOAdminDriver
{
	string steam_id;
	string name;
	int at;        // GetGame().GetTime() of the moment the driver sat down or was last seen driving
}

class DZOAdminDrivers
{
	static ref map<string, ref DZOAdminDriver> s_Last = new map<string, ref DZOAdminDriver>;

	static void Remember(string id, PlayerBase player)
	{
		PlayerIdentity ident = player.GetIdentity();
		if (!ident)
			return;
		DZOAdminDriver d = new DZOAdminDriver();
		d.steam_id = ident.GetPlainId();
		d.name = ident.GetName();
		d.at = GetGame().GetTime();
		s_Last.Set(id, d);
	}

	// Called from OnDriverEnter of every vehicle class, on the server only.
	static void Entered(Transport t, Human player)
	{
		PlayerBase pb = PlayerBase.Cast(player);
		if (!t || !pb || !GetGame().IsServer())
			return;
		Remember(DZOAdminWorld.VehicleId(t), pb);
	}

	// Entries go with the vehicle.
	static void Forget(Transport t)
	{
		if (t && s_Last)
			s_Last.Remove(DZOAdminWorld.VehicleId(t));
	}

	// Fills driver, driver_name and driven_ago of v. A driver sitting in the seat now drives now
	// (driven_ago 0, and the remembered entry is refreshed); otherwise the remembered entry counts.
	static void Fill(Transport t, DZOAdminVehicle v)
	{
		PlayerBase now = PlayerBase.Cast(t.CrewDriver());
		if (now && now.GetIdentity())
			Remember(v.id, now);
		DZOAdminDriver d;
		if (!s_Last.Find(v.id, d) || !d)
			return;
		v.driver = d.steam_id;
		v.driver_name = d.name;
		v.driven_ago = (GetGame().GetTime() - d.at) / 1000.0;
		if (v.driven_ago < 0)
			v.driven_ago = 0;
	}
}
