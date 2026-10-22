// SPDX-FileCopyrightText: 2026 Bernd Zeimetz <bernd@bzed.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

// Rebuilt houses and wells on the map (DayZ 1.30 rebuilding): one marker for every rebuildable map building with
// at least one part built, on the layer "Rebuilt houses" or, for a well, "Rebuilt wells". The marker's ttl is the
// time until decay has worn the first of its parts down to its floor. Decay does not remove a part, it stops at
// a fraction of the part's health. Operators and admins see them (a rebuilt house is a base).
class DZOAdminRebuilt
{
	static const string LAYER_HOUSES = "Rebuilt houses";
	static const string LAYER_WELLS = "Rebuilt wells";
	// The tunnel entrances (Land_IrrigationTunnel_Entrance_01 on Nasdara) are holes a rope ladder can be put on:
	// they are shown always, in another colour once the ladder is built and usable.
	static const string LAYER_TUNNELS = "Tunnel entrances";
	static const string COLOR_TUNNEL = "#6b5b45";
	static const string COLOR_LADDER = "#2a8553";

	protected static void Prop(DZOAdminMarker m, string k, string v)
	{
		DZOAdminKV kv = new DZOAdminKV();
		kv.k = k;
		kv.v = v;
		m.props.Insert(kv);
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

	// The id is the persistent id of the building when it has one, else its type and place: a map building does
	// not move.
	static string BuildingId(BuildingBase b)
	{
		int b1;
		int b2;
		int b3;
		int b4;
		b.GetPersistentID(b1, b2, b3, b4);
		if (b1 != 0 || b2 != 0 || b3 != 0 || b4 != 0)
			return "r" + b1.ToString() + "." + b2.ToString() + "." + b3.ToString() + "." + b4.ToString();
		vector p = b.GetPosition();
		int x = Math.Round(p[0]);
		int z = Math.Round(p[2]);
		return "r" + b.GetType() + "@" + x.ToString() + "," + z.ToString();
	}

	protected static void Tunnel(array<ref DZOAdminMarker> result, array<ref DZOAdminLayer> layers, BuildingBase b, bool ladder)
	{
		string color = COLOR_TUNNEL;
		if (ladder)
			color = COLOR_LADDER;
		DZOAdminLayer l = null;
		foreach (DZOAdminLayer have : layers)
		{
			if (have.name == LAYER_TUNNELS)
				l = have;
		}
		if (!l)
		{
			l = new DZOAdminLayer();
			l.name = LAYER_TUNNELS;
			l.icon = "dot-circle-o";
			l.color = COLOR_TUNNEL;
			l.visibility = "viewer";
			layers.Insert(l);
		}
		vector p = b.GetPosition();
		DZOAdminMarker m = new DZOAdminMarker();
		m.layer = LAYER_TUNNELS;
		m.id = BuildingId(b);
		m.shape = "point";
		m.x = p[0];
		m.y = p[1];
		m.z = p[2];
		m.icon = "dot-circle-o";
		m.color = color;
		m.label = "Tunnel entrance";
		m.props = new array<ref DZOAdminKV>;
		if (ladder)
			Prop(m, "ladder", "usable");
		else
			Prop(m, "ladder", "none");
		m.visibility = "viewer";
		m.source = "tunnel";
		result.Insert(m);
	}

	// A sandstorm makes decay faster (ConstructionBase.GetCurrentDecayCoef does the same call).
	protected static bool SandstormActive()
	{
		Weather w = GetGame().GetWeather();
		if (!w || !w.GetSandstorm())
			return false;
		return w.GetSandstorm().IsActive();
	}

	static void Collect(array<ref DZOAdminMarker> result, array<ref DZOAdminLayer> layers)
	{
		bool decayOn = ConstructionBasic.CanConstructionDecay();
		bool storm = SandstormActive();
		float coef = ConstructionBase.DEFAULT_DECAY_RATE_COEF;
		if (storm)
			coef = ConstructionBase.SANDSTORM_DECAY_RATE_COEF;
		foreach (BuildingBase b : DZOAdminRegistry.s_Rebuildables)
		{
			if (!b)
				continue;
			ConstructionBase con = ConstructionBase.Cast(b.GetConstructionBasic());
			if (!con)
				continue;
			map<string, ref ConstructionPart> parts = con.GetConstructionParts();
			if (!parts)
				continue;
			int total = 0;
			int built = 0;
			int ttl = 0;
			bool decaying = false;
			string weakest = "";
			float weakPct = 101;
			float weakFloor = 0;
			foreach (string name, ConstructionPart part : parts)
			{
				if (!part || part.IsUtilityPart())
					continue;
				total++;
				if (!part.IsBuilt())
					continue;
				built++;
				string zone;
				DamageSystem.GetDamageZoneFromComponentName(b, part.GetPartName(), zone);
				if (zone == "")
					continue;
				float maxHP = b.GetMaxHealth(zone, "Health");
				if (maxHP <= 0)
					continue;
				float hp = b.GetHealth(zone, "Health");
				float pct = hp / maxHP * 100;
				if (pct < weakPct)
				{
					weakPct = pct;
					weakest = part.GetPartName();
					weakFloor = part.GetPartDecayThreshold() * 100;
				}
				if (decayOn && part.CanPartDecay())
				{
					decaying = true;
					float floorHP = part.GetPartDecayThreshold() * maxHP;
					float rate = -part.GetPartDecayRate() * coef;
					if (hp > floorHP && rate > 0)
					{
						int left = Math.Ceil((hp - floorHP) / rate);
						if (left < 1)
							left = 1;
						if (ttl == 0 || left < ttl)
							ttl = left;
					}
				}
			}
			if (b.GetType().IndexOf("IrrigationTunnel") >= 0)
			{
				bool ladder = false;
				ConstructionPart lp = con.GetConstructionPart("ladder_rebuild");
				if (lp && lp.IsBuilt())
				{
					string lz;
					DamageSystem.GetDamageZoneFromComponentName(b, lp.GetPartName(), lz);
					ladder = lz == "" || b.GetHealth(lz, "Health") > 0;
				}
				Tunnel(result, layers, b, ladder);
				continue;
			}
			if (built == 0)
				continue;

			string layer = LAYER_HOUSES;
			string icon = "home";
			string color = "#8f6f00";
			// A well is a well by its class: Land_WaterWell_Brick only gives water once its roof is built.
			bool isWell = Well.Cast(b) != null;
			if (isWell)
			{
				layer = LAYER_WELLS;
				icon = "tint";
				color = "#2f6fb5";
			}
			Layer(layers, layer, icon, color);
			vector p = b.GetPosition();
			DZOAdminMarker m = new DZOAdminMarker();
			m.layer = layer;
			m.id = BuildingId(b);
			m.shape = "point";
			m.x = p[0];
			m.y = p[1];
			m.z = p[2];
			m.icon = icon;
			m.color = color;
			m.label = b.GetDisplayName();
			if (m.label == "")
				m.label = b.GetType();
			m.props = new array<ref DZOAdminKV>;
			m.visibility = "operator";
			m.source = "rebuilt";
			Prop(m, "parts", built.ToString() + "/" + total.ToString());
			if (weakest != "")
			{
				Prop(m, "weakest", weakest + " " + Math.Round(weakPct).ToString() + "%");
				Prop(m, "floor", Math.Round(weakFloor).ToString() + "%");
			}
			// A ttl of 0 means "no expiry" on the wire, so what is left to say goes in a prop.
			if (!decayOn)
				Prop(m, "decay", "off");
			else if (ttl > 0)
				m.ttl = ttl;
			else if (decaying)
				Prop(m, "decay", "worn down");
			if (storm && ttl > 0)
				Prop(m, "sandstorm", "decaying faster");
			if (isWell)
			{
				if (b.GetWaterSourceObjectType() == EWaterSourceObjectType.WELL)
					Prop(m, "water", "drinkable");
				else
					Prop(m, "water", "none yet");
			}
			string locks = LockText(b);
			if (locks != "")
				Prop(m, "locks", locks);
			result.Insert(m);
		}
	}

	// The code locks and combination locks the building carries, "2 code locks, 1 combination lock".
	protected static string LockText(BuildingBase b)
	{
		GameInventory inv = b.GetInventory();
		if (!inv)
			return "";
		int code = 0;
		int combination = 0;
		for (int i = 0; i < inv.AttachmentCount(); i++)
		{
			EntityAI att = inv.GetAttachmentFromIndex(i);
			if (DigitalCodeLock.Cast(att))
				code++;
			else if (CombinationLock.Cast(att))
				combination++;
		}
		string text = "";
		if (code == 1)
			text = "1 code lock";
		else if (code > 1)
			text = code.ToString() + " code locks";
		if (combination > 0)
		{
			if (text != "")
				text += ", ";
			if (combination == 1)
				text += "1 combination lock";
			else
				text += combination.ToString() + " combination locks";
		}
		return text;
	}
}
