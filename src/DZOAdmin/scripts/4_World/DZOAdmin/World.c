// SPDX-FileCopyrightText: 2026 Bernd Zeimetz <bernd@bzed.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

// Reads the game state for the map and carries out the admin actions. All
// engine calls are server-side; every accessor is null-checked.
class DZOAdminWorld
{
	static string VehicleId(EntityAI e)
	{
		int b1;
		int b2;
		int b3;
		int b4;
		e.GetPersistentID(b1, b2, b3, b4);
		return b1.ToString() + "." + b2.ToString() + "." + b3.ToString() + "." + b4.ToString();
	}

	static PlayerBase FindPlayer(string steamId)
	{
		if (steamId == "")
			return null;
		array<Man> men = new array<Man>;
		GetGame().GetPlayers(men);
		foreach (Man m : men)
		{
			PlayerBase p = PlayerBase.Cast(m);
			if (p && p.GetIdentity() && p.GetIdentity().GetPlainId() == steamId)
				return p;
		}
		return null;
	}

	static CarScript FindVehicle(string id)
	{
		foreach (CarScript c : DZOAdminRegistry.s_Cars)
		{
			if (c && VehicleId(c) == id)
				return c;
		}
		return null;
	}

	static void CollectPlayers(array<ref DZOAdminPlayer> out)
	{
		array<Man> men = new array<Man>;
		GetGame().GetPlayers(men);
		foreach (Man m : men)
		{
			PlayerBase p = PlayerBase.Cast(m);
			if (!p)
				continue;
			PlayerIdentity id = p.GetIdentity();
			if (!id)
				continue;
			DZOAdminPlayer d = new DZOAdminPlayer();
			d.steam_id = id.GetPlainId();
			d.name = id.GetName();
			d.slot = id.GetPlayerId();
			d.ping = id.GetPingAvg();
			vector pos = p.GetPosition();
			d.x = pos[0];
			d.y = pos[1];
			d.z = pos[2];
			d.yaw = p.GetOrientation()[0];
			d.health = p.GetHealth("GlobalHealth", "Health");
			d.blood = p.GetHealth("GlobalHealth", "Blood");
			d.shock = p.GetHealth("GlobalHealth", "Shock");
			d.alive = p.IsAlive();
			d.unconscious = p.IsUnconscious();
			out.Insert(d);
		}
	}

	static void CollectVehicles(array<ref DZOAdminVehicle> out)
	{
		foreach (CarScript c : DZOAdminRegistry.s_Cars)
		{
			if (!c)
				continue;
			DZOAdminVehicle v = new DZOAdminVehicle();
			v.id = VehicleId(c);
			v.type = c.GetType();
			vector pos = c.GetPosition();
			v.x = pos[0];
			v.y = pos[1];
			v.z = pos[2];
			v.yaw = c.GetOrientation()[0];
			v.health = c.GetHealth01("", "");
			v.fuel = c.GetFluidFraction(CarFluid.FUEL);
			v.ruined = c.IsRuined();
			v.engine = c.EngineIsOn();
			v.occupants = new array<string>;
			for (int i = 0; i < c.CrewSize(); i++)
			{
				PlayerBase crew = PlayerBase.Cast(c.CrewMember(i));
				if (crew && crew.GetIdentity())
					v.occupants.Insert(crew.GetIdentity().GetPlainId());
			}
			out.Insert(v);
		}
	}

	static void CollectEvents(array<ref DZOAdminEvent> out)
	{
		foreach (EffectArea a : DZOAdminRegistry.s_Areas)
		{
			if (!a)
				continue;
			DZOAdminEvent e = new DZOAdminEvent();
			e.id = "area" + a.GetID();
			e.name = a.GetType();
			e.category = "effect_area";
			vector pos = a.GetPosition();
			e.x = pos[0];
			e.y = pos[1];
			e.z = pos[2];
			e.radius = a.m_Radius;
			e.source = "effect_area";
			out.Insert(e);
		}
	}

	// SpawnableTypes lists the public item classes of every loaded mod, for
	// dzo's item picker.
	static void SpawnableTypes(array<string> out)
	{
		TStringArray roots = {"CfgVehicles", "CfgWeapons", "CfgMagazines"};
		foreach (string root : roots)
		{
			int n = GetGame().ConfigGetChildrenCount(root);
			for (int i = 0; i < n; i++)
			{
				string name;
				GetGame().ConfigGetChildName(root, i, name);
				if (GetGame().ConfigGetInt(root + " " + name + " scope") != 2)
					continue;
				// Only things that can live in an inventory or on the ground.
				if (root == "CfgVehicles" && !GetGame().IsKindOf(name, "Inventory_Base") && !GetGame().IsKindOf(name, "Clothing_Base"))
					continue;
				out.Insert(name);
			}
		}
	}

	// ---- actions; each returns "" on success or an error text ----

	static string Message(DZOAdminCommand c)
	{
		string style = "colorStatusChannel";
		if (c.style == "important")
			style = "colorImportant";
		array<Man> men = new array<Man>;
		if (c.steam_id == "")
		{
			GetGame().GetPlayers(men);
		}
		else
		{
			PlayerBase one = FindPlayer(c.steam_id);
			if (!one)
				return "player not online";
			men.Insert(one);
		}
		int sent = 0;
		foreach (Man m : men)
		{
			PlayerBase p = PlayerBase.Cast(m);
			if (!p || !p.GetIdentity())
				continue;
			if (c.style == "notification")
				NotificationSystem.SendNotificationToPlayerIdentityExtended(p.GetIdentity(), 10, "Server", c.text, "");
			else
				p.Message(c.text, style);
			sent++;
		}
		if (sent == 0)
			return "nobody to deliver to";
		return "";
	}

	static string Teleport(DZOAdminCommand c)
	{
		PlayerBase p = FindPlayer(c.steam_id);
		if (!p)
			return "player not online";
		if (!p.IsAlive())
			return "player is dead";
		if (p.IsInTransport())
			return "player is in a vehicle";
		vector pos;
		if (c.to_steam_id != "")
		{
			PlayerBase to = FindPlayer(c.to_steam_id);
			if (!to)
				return "target player not online";
			pos = to.GetPosition();
		}
		else
		{
			pos[0] = c.x;
			pos[2] = c.z;
			if (c.has_y)
				pos[1] = c.y;
			else
				pos[1] = GetGame().SurfaceY(c.x, c.z);
		}
		p.SetPosition(pos);
		return "";
	}

	static string SpawnItem(DZOAdminCommand c, DZOAdminConfig cfg)
	{
		if (!cfg.SpawnAllowed(c.type))
			return "class not allowed";
		if (!GetGame().ConfigIsExisting("CfgVehicles " + c.type) && !GetGame().ConfigIsExisting("CfgWeapons " + c.type) && !GetGame().ConfigIsExisting("CfgMagazines " + c.type))
			return "unknown class";
		PlayerBase p = FindPlayer(c.steam_id);
		if (!p)
			return "player not online";
		if (!p.IsAlive())
			return "player is dead";
		EntityAI item = null;
		if (c.target == "hands")
		{
			item = p.GetHumanInventory().CreateInHands(c.type);
		}
		else if (c.target != "ground")
		{
			item = p.GetInventory().CreateInInventory(c.type);
		}
		// Full inventory or "ground": drop it at the player's feet.
		if (!item)
			item = EntityAI.Cast(GetGame().CreateObjectEx(c.type, p.GetPosition(), ECE_PLACE_ON_SURFACE));
		if (!item)
			return "could not create the item";
		ItemBase ib = ItemBase.Cast(item);
		if (ib && c.quantity > 0)
			ib.SetQuantity(c.quantity);
		if (c.health > 0 && c.health <= 1)
			item.SetHealth01("", "", c.health);
		return "";
	}

	protected static void RepairEntity(EntityAI e)
	{
		TStringArray zones = new TStringArray;
		e.GetDamageZones(zones);
		e.SetHealthMax("", "");
		foreach (string z : zones)
			e.SetHealthMax(z, "");
	}

	protected static void FillFluid(CarScript car, CarFluid fluid)
	{
		float missing = car.GetFluidCapacity(fluid) * (1 - car.GetFluidFraction(fluid));
		if (missing > 0)
			car.Fill(fluid, missing);
	}

	static string VehicleRepair(DZOAdminCommand c)
	{
		CarScript car = FindVehicle(c.vehicle);
		if (!car)
			return "vehicle not found";
		string scope = c.scope;
		if (scope == "")
			scope = "all";
		if (scope == "all" || scope == "engine")
			RepairEntity(car);
		if (scope == "all" || scope == "parts" || scope == "wheels")
		{
			GameInventory inv = car.GetInventory();
			for (int i = 0; inv && i < inv.AttachmentCount(); i++)
			{
				EntityAI part = inv.GetAttachmentFromIndex(i);
				if (!part)
					continue;
				bool wheel = part.IsKindOf("CarWheel");
				if (scope == "all" || (scope == "wheels" && wheel) || (scope == "parts" && !wheel))
					RepairEntity(part);
			}
		}
		if (scope == "all" || scope == "fluids")
		{
			FillFluid(car, CarFluid.FUEL);
			FillFluid(car, CarFluid.OIL);
			FillFluid(car, CarFluid.BRAKE);
			FillFluid(car, CarFluid.COOLANT);
		}
		return "";
	}

	static string VehicleDelete(DZOAdminCommand c)
	{
		CarScript car = FindVehicle(c.vehicle);
		if (!car)
			return "vehicle not found";
		if (!c.force && car.IsAnyCrewPresent())
			return "vehicle has crew inside";
		GetGame().ObjectDelete(car);
		return "";
	}
}
