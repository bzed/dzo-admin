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
		// The "v" keeps ids that start with a minus sign from looking like
		// command line flags.
		return "v" + b1.ToString() + "." + b2.ToString() + "." + b3.ToString() + "." + b4.ToString();
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

	static void CollectPlayers(array<ref DZOAdminPlayer> list)
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
			list.Insert(d);
		}
	}

	static void CollectVehicles(array<ref DZOAdminVehicle> list)
	{
		foreach (CarScript c : DZOAdminRegistry.s_Cars)
		{
			if (!c)
				continue;
			DZOAdminVehicle v = new DZOAdminVehicle();
			v.id = VehicleId(c);
			v.kind = "car";
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
#ifdef EXPANSIONMODVEHICLE
			ExpansionVehicle ev = c.GetExpansionVehicle();
			if (ev)
				v.owner = ev.GetOwnerName();
#endif
			list.Insert(v);
		}
#ifdef EXPANSIONMODVEHICLE
		CollectCovers(list);
#endif
#ifndef DAYZ_1_29
		DZOAdminBikes.Collect(list);
#endif
	}

#ifdef EXPANSIONMODVEHICLE
	// DayZ Expansion stores a covered vehicle in a file and leaves an ExpansionVehicleCover in the
	// world, which is no CarScript. The cover is reported as the vehicle it holds (its class is the
	// first entry of the cover's netsync data), without health, fuel or crew.
	static void CollectCovers(array<ref DZOAdminVehicle> list)
	{
		foreach (ExpansionEntityStoragePlaceholder p : ExpansionEntityStoragePlaceholder.s_Expansion_AllPlaceholders)
		{
			ExpansionVehicleCover cover = ExpansionVehicleCover.Cast(p);
			if (!cover || !cover.m_Expansion_NetsyncData)
				continue;
			string type;
			if (!cover.m_Expansion_NetsyncData.Get(0, type) || type == "")
				continue;
			DZOAdminVehicle v = new DZOAdminVehicle();
			v.id = VehicleId(cover);
			v.kind = "car";
			v.type = type;
			vector pos = cover.GetPosition();
			v.x = pos[0];
			v.y = pos[1];
			v.z = pos[2];
			v.yaw = cover.GetOrientation()[0];
			v.occupants = new array<string>;
			v.owner = cover.Expansion_GetOwnerName();
			v.covered = true;
			list.Insert(v);
		}
	}
#endif

	static void CollectEvents(array<ref DZOAdminEvent> list)
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
			list.Insert(e);
		}
	}

	// SpawnableTypes lists the item classes of every loaded mod that are safe to spawn, for dzo's
	// item picker: public (scope 2) classes that can live in an inventory or on the ground, without
	// the ones that crash the server or raise script errors when they are created. Those were
	// found by creating every class on a server, see scripts/test-spawnable.sh (game 1.30). A mod's
	// own broken classes are not known here: run that script for a server with mods.
	static void SpawnableTypes(array<string> list)
	{
		TStringArray roots = {"CfgVehicles", "CfgWeapons", "CfgMagazines"};
		TStringArray unsafe = {
			// crash the server
			"PVCBow", "QuickieBow", "RecurveBow", "GP25", "GP25_Standalone", "M203", "M203_Standalone", "DartGun",
			// script errors
			"Ammo_40mm_Smoke_Black", "Fence", "Groza", "LargeTentBackPack", "LAW", "M249", "RPG7", "Shockpistol",
			"Shockpistol_Black", "TerritoryFlag", "Trumpet", "Watchtower"};
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
				if (unsafe.Find(name) > -1)
					continue;
				list.Insert(name);
			}
		}
	}

	// SpawnableVehicles lists the vehicle classes (public cars, boats and helicopters, of every loaded
	// mod) for the vehicle picker of the map.
	static void SpawnableVehicles(array<string> list)
	{
		int n = GetGame().ConfigGetChildrenCount("CfgVehicles");
		for (int i = 0; i < n; i++)
		{
			string name;
			GetGame().ConfigGetChildName("CfgVehicles", i, name);
			if (GetGame().ConfigGetInt("CfgVehicles " + name + " scope") == 2 && GetGame().IsKindOf(name, "Transport"))
				list.Insert(name);
		}
	}

	// ---- actions; each returns "" on success or an error text ----

	static string Message(DZOAdminCommand c)
	{
		string style = "colorAction";
		if (c.style == "blue")
			style = "colorStatusChannel";
		else if (c.style == "important")
			style = "colorImportant";
		else if (c.style == "friendly")
			style = "colorFriendly";
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
				NotificationSystem.SendNotificationToPlayerIdentityExtended(p.GetIdentity(), Seconds(c), Title(c), c.text, "");
			else if (c.style == "expansion")
			{
				string failed = SendExpansion(p.GetIdentity(), c);
				if (failed != "")
					return failed;
			}
			else
				p.Message(c.text, style);
			sent++;
		}
		if (sent == 0)
			return "nobody to deliver to";
		return "";
	}

	// An older dzo sends no title or duration: the mod then uses these.
	static string Title(DZOAdminCommand c)
	{
		if (c.title == "")
			return "Server";
		return c.title;
	}

	static int Seconds(DZOAdminCommand c)
	{
		if (c.seconds <= 0)
			return 10;
		return c.seconds;
	}

	// The Expansion notification of the type c.notify_type, or an error text when Expansion is not loaded.
	static string SendExpansion(PlayerIdentity id, DZOAdminCommand c)
	{
#ifdef EXPANSIONMODCORE
		ExpansionNotificationType type = ExpansionNotificationType.TOAST;
		switch (c.notify_type)
		{
		case "baguette": type = ExpansionNotificationType.BAGUETTE; break;
		case "activity": type = ExpansionNotificationType.ACTIVITY; break;
		case "killfeed": type = ExpansionNotificationType.KILLFEED; break;
		case "market": type = ExpansionNotificationType.MARKET; break;
		case "garage": type = ExpansionNotificationType.GARAGE; break;
		}
		ExpansionNotification(Title(c), c.text, "", ARGB(255, 52, 152, 219), Seconds(c), type).Create(id);
		return "";
#else
		return "expansion notifications are not available";
#endif
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

	static string PlayerKill(DZOAdminCommand c)
	{
		PlayerBase p = FindPlayer(c.steam_id);
		if (!p)
			return "player not online";
		if (!p.IsAlive())
			return "player is already dead";
		p.SetHealth("", "", -1); // what the game's own kill command does
		return "";
	}

	static string PlayerHeal(DZOAdminCommand c)
	{
		PlayerBase p = FindPlayer(c.steam_id);
		if (!p)
			return "player not online";
		if (!p.IsAlive())
			return "player is dead";
		p.DZOAdminHeal();
		return "";
	}

	// Repairs everything the player carries and wears, the parts of an item as well.
	static string PlayerFixGear(DZOAdminCommand c)
	{
		PlayerBase p = FindPlayer(c.steam_id);
		if (!p)
			return "player not online";
		if (!p.IsAlive())
			return "player is dead";
		p.FixAllInventoryItems(); // health, wetness, temperature and frost of every item
		array<EntityAI> items = new array<EntityAI>;
		p.GetInventory().EnumerateInventory(InventoryTraversalType.INORDER, items);
		foreach (EntityAI item : items)
			RepairEntity(item); // and the damage zones
		return "";
	}

	// The most stacks one spawn makes: a quantity of 20 bricks is as many stacks as the stack size needs.
	static const int MAX_STACKS = 50;

	// The point on the ground at x/z.
	static vector GroundAt(float x, float z)
	{
		return Vector(x, GetGame().SurfaceY(x, z), z);
	}

	// Spawns an item for a player (target inventory, hands or ground), or on the ground at x/z when no
	// player is named. A quantity above what a stack holds makes more stacks, up to MAX_STACKS.
	static string SpawnItem(DZOAdminCommand c, DZOAdminConfig cfg)
	{
		if (!cfg.SpawnAllowed(c.type))
			return "class not allowed";
		if (!GetGame().ConfigIsExisting("CfgVehicles " + c.type) && !GetGame().ConfigIsExisting("CfgWeapons " + c.type) && !GetGame().ConfigIsExisting("CfgMagazines " + c.type))
			return "unknown class";
		PlayerBase p = null;
		vector pos;
		string target = c.target;
		if (c.steam_id != "")
		{
			p = FindPlayer(c.steam_id);
			if (!p)
				return "player not online";
			if (!p.IsAlive())
				return "player is dead";
			pos = p.GetPosition();
		}
		else
		{
			pos = GroundAt(c.x, c.z);
			target = "ground";
		}
		// quantity: -1 = leave what the game makes, -2 = one full stack (the class's own maximum), 0 or more is
		// passed to SetQuantity (0 makes the item empty; a class that cannot exist empty is deleted by the game).
		bool fill = c.quantity == -2;
		bool keep = c.quantity < 0 && !fill;
		float left = c.quantity;
		int made = 0;
		while (true)
		{
			EntityAI item = null;
			if (p && target == "hands" && made == 0)
			{
				item = p.GetHumanInventory().CreateInHands(c.type);
			}
			else if (p && target != "ground")
			{
				item = p.GetInventory().CreateInInventory(c.type);
			}
			// Full inventory or "ground": drop it at the player's feet.
			if (!item)
				item = EntityAI.Cast(GetGame().CreateObjectEx(c.type, pos, ECE_PLACE_ON_SURFACE));
			if (!item)
			{
				if (made == 0)
					return "could not create the item";
				return "made " + made.ToString() + " stacks, then the world refused more";
			}
			made++;
			ItemBase ib = ItemBase.Cast(item);
			float max = 0;
			if (ib)
				max = ib.GetQuantityMax();
			if (ib && fill)
			{
				if (max > 0)
					ib.SetQuantity(max);
				left = 0;
			}
			else if (ib && !keep)
			{
				if (made == 1 && max > 0 && Math.Ceil(left / max) > MAX_STACKS)
				{
					GetGame().ObjectDelete(item);
					return "that quantity needs more than " + MAX_STACKS.ToString() + " stacks of " + max.ToString();
				}
				float q = left;
				if (max > 0 && q > max)
					q = max;
				ib.SetQuantity(q);
				left = left - q;
			}
			if (c.health > 0 && c.health <= 1)
				item.SetHealth01("", "", c.health);
			// The game's own debug spawn: every item class that needs parts to be usable knows them. When
			// that leaves the item bare, fit the parts found in the configs (DZOAdminKits).
			if (c.attachments)
			{
				item.OnDebugSpawn();
				if (item.GetInventory().AttachmentCount() == 0)
					DZOAdminKits.Fit(item, 0);
			}
			if (left <= 0 || max <= 0)
				return "";
		}
		return "";
	}

	// Spawns a vehicle on the ground at x/z, the way the game's debug spawn makes it: with its parts, fuel
	// and the tools to repair it.
	static string SpawnVehicle(DZOAdminCommand c, DZOAdminConfig cfg)
	{
		if (!cfg.SpawnAllowed(c.type))
			return "class not allowed";
		if (!GetGame().ConfigIsExisting("CfgVehicles " + c.type) || !GetGame().IsKindOf(c.type, "Transport"))
			return "not a vehicle class";
		EntityAI v = EntityAI.Cast(GetGame().CreateObjectEx(c.type, GroundAt(c.x, c.z), ECE_PLACE_ON_SURFACE | ECE_CREATEPHYSICS | ECE_UPDATEPATHGRAPH));
		if (!v)
			return "could not create the vehicle";
		v.OnDebugSpawn();
		return "";
	}

	static void RepairEntity(EntityAI e)
	{
		TStringArray zones = new TStringArray;
		e.GetDamageZones(zones);
		e.SetHealthMax("", "");
		foreach (string z : zones)
			e.SetHealthMax(z, "");
	}

	// Fits parts into the slots that are empty. A vehicle of the same class is created out of sight and
	// given its debug-spawn parts (what vanilla's own admin tools do); whatever it carries in a slot
	// that is empty on the real vehicle is created there as well, then the donor is deleted again.
	static void AddMissingParts(EntityAI e)
	{
		vector pos = e.GetPosition();
		pos[1] = pos[1] + 500;
		EntityAI donor = EntityAI.Cast(GetGame().CreateObjectEx(e.GetType(), pos, ECE_NONE));
		if (!donor)
			return;
		donor.OnDebugSpawn();
		GameInventory src = donor.GetInventory();
		GameInventory dst = e.GetInventory();
		for (int i = 0; src && dst && i < src.AttachmentCount(); i++)
		{
			EntityAI part = src.GetAttachmentFromIndex(i);
			if (!part)
				continue;
			// The slot the donor fits the part into, not the first slot the part could go into: a
			// headlight fits two slots and a car has one of each.
			InventoryLocation loc = new InventoryLocation();
			if (!part.GetInventory().GetCurrentInventoryLocation(loc) || loc.GetType() != InventoryLocationType.ATTACHMENT)
				continue;
			int slot = loc.GetSlot();
			if (!dst.FindAttachment(slot))
				dst.CreateAttachmentEx(part.GetType(), slot);
		}
		FillWheelSlots(e, donor);
		GetGame().ObjectDelete(donor);
	}

	// Every wheel slot the vehicle's config lists (the four wheels and the spare, where it has them)
	// gets a wheel: the debug spawn may leave one out. The wheel class comes from the wheels the
	// vehicle or the donor carries; each is tried in the empty slot until one fits.
	protected static void FillWheelSlots(EntityAI e, EntityAI donor)
	{
		TStringArray types = new TStringArray;
		GameInventory src = donor.GetInventory();
		GameInventory dst = e.GetInventory();
		for (int i = 0; i < dst.AttachmentCount(); i++)
		{
			EntityAI w = dst.GetAttachmentFromIndex(i);
			if (w && w.IsKindOf("CarWheel") && types.Find(w.GetType()) < 0)
				types.Insert(w.GetType());
		}
		for (int j = 0; j < src.AttachmentCount(); j++)
		{
			EntityAI dw = src.GetAttachmentFromIndex(j);
			if (dw && dw.IsKindOf("CarWheel") && types.Find(dw.GetType()) < 0)
				types.Insert(dw.GetType());
		}
		if (types.Count() == 0)
			return;
		TStringArray names = new TStringArray;
		GetGame().ConfigGetTextArray("CfgVehicles " + e.GetType() + " attachments", names);
		foreach (string name : names)
		{
			if (!name.Contains("Wheel"))
				continue;
			int slot = InventorySlots.GetSlotIdFromString(name);
			if (slot == InventorySlots.INVALID || dst.FindAttachment(slot))
				continue;
			foreach (string type : types)
			{
				if (dst.CreateAttachmentEx(type, slot))
					break;
			}
		}
	}

	// Stands a vehicle upright on its wheels where it is, keeping its heading.
	static void FlipEntity(EntityAI e)
	{
		vector pos = e.GetPosition();
		pos[1] = Math.Max(pos[1], GetGame().SurfaceY(pos[0], pos[2])) + 1.0;
		float yaw = e.GetOrientation()[0];
		e.SetPosition(pos);
		e.SetOrientation(Vector(yaw, 0, 0));
		dBodySetAngularVelocity(e, "0 0 0");
		dBodyActive(e, ActiveState.ACTIVE);
	}

	protected static void FillFluid(CarScript car, CarFluid fluid)
	{
		float missing = car.GetFluidCapacity(fluid) * (1 - car.GetFluidFraction(fluid));
		if (missing > 0)
			car.Fill(fluid, missing);
	}

	static string VehicleRepair(DZOAdminCommand c)
	{
		string scope = c.scope;
		if (scope == "")
			scope = "all";
		CarScript car = FindVehicle(c.vehicle);
		if (!car)
		{
#ifndef DAYZ_1_29
			MotorbikeScript bike = DZOAdminBikes.Find(c.vehicle);
			if (bike)
				return DZOAdminBikes.Repair(bike, scope);
#endif
			return "vehicle not found";
		}
		if (scope == "all" || scope == "missing")
			AddMissingParts(car);
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

	static string VehicleFlip(DZOAdminCommand c)
	{
		CarScript car = FindVehicle(c.vehicle);
		if (car)
		{
			FlipEntity(car);
			return "";
		}
#ifndef DAYZ_1_29
		MotorbikeScript bike = DZOAdminBikes.Find(c.vehicle);
		if (bike)
		{
			FlipEntity(bike);
			return "";
		}
#endif
		return "vehicle not found";
	}

	static string VehicleDelete(DZOAdminCommand c)
	{
		CarScript car = FindVehicle(c.vehicle);
		if (!car)
		{
#ifndef DAYZ_1_29
			MotorbikeScript bike = DZOAdminBikes.Find(c.vehicle);
			if (bike)
			{
				if (!c.force && bike.IsAnyCrewPresent())
					return "vehicle has crew inside";
				GetGame().ObjectDelete(bike);
				return "";
			}
#endif
			return "vehicle not found";
		}
		if (!c.force && car.IsAnyCrewPresent())
			return "vehicle has crew inside";
		GetGame().ObjectDelete(car);
		return "";
	}
}
