// SPDX-FileCopyrightText: 2026 Bernd Zeimetz <bernd@bzed.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

// Boats of the game (1.30): BoatScript is not a CarScript, so CarScript's registry in Hooks.c never sees
// them. They are kept in a registry of their own and reported as vehicles of kind "boat". The boats of
// DayZ Expansion (ExpansionBoatScript) are cars to the game and are reported as boats by World.c; Expansion's
// Boat_01 is a BoatScript and is found here. 1.29 is left out like the motorbikes.
#ifndef DAYZ_1_29
class DZOAdminBoats
{
	static ref array<BoatScript> s_Boats = new array<BoatScript>;

	static BoatScript Find(string id)
	{
		foreach (BoatScript b : s_Boats)
		{
			if (b && DZOAdminWorld.VehicleId(b) == id)
				return b;
		}
		return null;
	}

	static void Collect(array<ref DZOAdminVehicle> list)
	{
		foreach (BoatScript b : s_Boats)
		{
			if (!b)
				continue;
			DZOAdminVehicle v = new DZOAdminVehicle();
			v.id = DZOAdminWorld.VehicleId(b);
			v.kind = "boat";
			v.type = b.GetType();
			vector pos = b.GetPosition();
			v.x = pos[0];
			v.y = pos[1];
			v.z = pos[2];
			v.yaw = b.GetOrientation()[0];
			v.health = b.GetHealth01("", "");
			v.fuel = b.GetFluidFraction(BoatFluid.FUEL);
			v.ruined = b.IsRuined();
			v.engine = b.EngineIsOn();
			v.occupants = new array<string>;
			for (int i = 0; i < b.CrewSize(); i++)
			{
				PlayerBase crew = PlayerBase.Cast(b.CrewMember(i));
				if (crew && crew.GetIdentity())
					v.occupants.Insert(crew.GetIdentity().GetPlainId());
			}
			DZOAdminDrivers.Fill(b, v);
			list.Insert(v);
		}
	}

	protected static void FillFluid(BoatScript boat, BoatFluid fluid)
	{
		float missing = boat.GetFluidCapacity(fluid) * (1 - boat.GetFluidFraction(fluid));
		if (missing > 0)
			boat.Fill(fluid, missing);
	}

	// Same scopes as for cars. A boat has no wheels and only fuel to fill (like the motorbike), so the
	// wheels and fluids scopes only touch what it has.
	static string Repair(BoatScript boat, string scope)
	{
		if (scope == "all" || scope == "missing")
			DZOAdminWorld.AddMissingParts(boat);
		if (scope == "all" || scope == "engine")
			DZOAdminWorld.RepairEntity(boat);
		if (scope == "all" || scope == "parts" || scope == "wheels")
		{
			GameInventory inv = boat.GetInventory();
			for (int i = 0; inv && i < inv.AttachmentCount(); i++)
			{
				EntityAI part = inv.GetAttachmentFromIndex(i);
				if (!part)
					continue;
				bool wheel = part.IsKindOf("CarWheel");
				if (scope == "all" || (scope == "wheels" && wheel) || (scope == "parts" && !wheel))
					DZOAdminWorld.RepairEntity(part);
			}
		}
		if (scope == "all" || scope == "fluids")
			FillFluid(boat, BoatFluid.FUEL);
		return "";
	}
}

modded class BoatScript
{
	override void EEInit()
	{
		super.EEInit();
		if (GetGame().IsServer())
		{
			DZOAdminBoats.s_Boats.Insert(this);
			if (DZOAdminWatch.s_Rules)
				DZOAdminWatch.OnEntityInit(this);
		}
	}

	override void EEDelete(EntityAI parent)
	{
		DZOAdminBoats.s_Boats.RemoveItem(this);
		DZOAdminDrivers.Forget(this);
		DZOAdminWatch.OnEntityDelete(this);
		super.EEDelete(parent);
	}

	override void OnDriverEnter(Human player)
	{
		super.OnDriverEnter(player);
		DZOAdminDrivers.Entered(this, player);
	}
}
#endif
