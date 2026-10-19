// SPDX-FileCopyrightText: 2026 Bernd Zeimetz <bernd@bzed.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

// Territory flags on the map: one marker per flag on the layer "Flags", whose ttl is the refresher
// time the flag has left, and, with DayZ Expansion, a circle per territory on the layer "Territories".
// Operators and admins see them (a flag is a base).
class DZOAdminFlags
{
	static const string LAYER_FLAGS = "Flags";
	static const string LAYER_TERRITORIES = "Territories";

	protected static void Prop(DZOAdminMarker m, string k, string v)
	{
		DZOAdminKV kv = new DZOAdminKV();
		kv.k = k;
		kv.v = v;
		m.props.Insert(kv);
	}

	// The id of a flag is its persistent id, so it stays the same over a restart of the server.
	static string FlagId(TerritoryFlag f)
	{
		int b1;
		int b2;
		int b3;
		int b4;
		f.GetPersistentID(b1, b2, b3, b4);
		return "f" + b1.ToString() + "." + b2.ToString() + "." + b3.ToString() + "." + b4.ToString();
	}

	// Raise puts the flag to the top of its pole and fills its refresher time, as a player does with a new flag.
	static string Raise(DZOAdminCommand c)
	{
		foreach (TerritoryFlag f : DZOAdminRegistry.s_Flags)
		{
			if (f && FlagId(f) == c.flag)
			{
				f.AddRefresherTime01(1);
				f.AnimateFlag(1 - f.GetRefresherTime01());
				return "";
			}
		}
		return "flag not found";
	}

	// Layer defines a layer once per call, so that /layers lists it for the roles that see its markers.
	protected static void Layer(array<ref DZOAdminLayer> layers, string name, string icon, string color)
	{
		foreach (DZOAdminLayer have : layers)
		{
			if (have.name == name)
				return;
		}
		DZOAdminLayer l = new DZOAdminLayer();
		l.name = name;
		l.icon = icon;
		l.color = color;
		l.visibility = "operator";
		layers.Insert(l);
	}

	static void Collect(array<ref DZOAdminMarker> result, array<ref DZOAdminLayer> layers)
	{
		foreach (TerritoryFlag f : DZOAdminRegistry.s_Flags)
		{
			if (!f)
				continue;
			vector p = f.GetPosition();
			DZOAdminMarker m = new DZOAdminMarker();
			Layer(layers, LAYER_FLAGS, "flag", "#c0392b");
			m.layer = LAYER_FLAGS;
			m.id = FlagId(f);
			m.shape = "point";
			m.x = p[0];
			m.y = p[1];
			m.z = p[2];
			m.icon = "flag";
			m.color = "#c0392b";
			m.label = "Flag";
			m.props = new array<ref DZOAdminKV>;
			m.visibility = "operator";
			m.source = "flags";
			// A ttl of 0 means "no expiry" on the wire, so an expired refresher is a prop.
			if (f.m_RefresherTimeRemaining > 0)
				m.ttl = f.m_RefresherTimeRemaining;
			else
				Prop(m, "refresher", "expired");
#ifdef EXPANSIONMODBASEBUILDING
			if (f.HasExpansionTerritoryInformation() && f.GetTerritory() && f.GetTerritory().GetTerritoryName() != "")
			{
				string name = f.GetTerritory().GetTerritoryName();
				Prop(m, "territory", name);
				DZOAdminMarker t = new DZOAdminMarker();
				Layer(layers, LAYER_TERRITORIES, "", "#2f6fb5");
				t.layer = LAYER_TERRITORIES;
				t.id = "t" + FlagId(f);
				t.shape = "circle";
				t.x = p[0];
				t.y = p[1];
				t.z = p[2];
				t.radius = GetExpansionSettings().GetTerritory().TerritorySize;
				t.color = "#2f6fb5";
				t.label = name;
				t.props = new array<ref DZOAdminKV>;
				t.visibility = "operator";
				t.source = "territory";
				result.Insert(t);
			}
#endif
			result.Insert(m);
		}
	}
}
