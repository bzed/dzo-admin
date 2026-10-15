// SPDX-FileCopyrightText: 2026 Bernd Zeimetz <bernd@bzed.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

// Motorbikes (1.30): Motorbike is not a Car, so CarScript's registry in Hooks.c never sees them.
// They are kept in a registry of their own and reported as vehicles of kind "motorbike". 1.29 has
// no motorbikes, and no Transport class either, so the whole file is left out there.
#ifndef DAYZ_1_29
class DZOAdminBikes
{
	static ref array<MotorbikeScript> s_Bikes = new array<MotorbikeScript>;

	static MotorbikeScript Find(string id)
	{
		foreach (MotorbikeScript b : s_Bikes)
		{
			if (b && DZOAdminWorld.VehicleId(b) == id)
				return b;
		}
		return null;
	}

	static void Collect(array<ref DZOAdminVehicle> list)
	{
		foreach (MotorbikeScript b : s_Bikes)
		{
			if (!b)
				continue;
			DZOAdminVehicle v = new DZOAdminVehicle();
			v.id = DZOAdminWorld.VehicleId(b);
			v.kind = "motorbike";
			v.type = b.GetType();
			vector pos = b.GetPosition();
			v.x = pos[0];
			v.y = pos[1];
			v.z = pos[2];
			v.yaw = b.GetOrientation()[0];
			v.health = b.GetHealth01("", "");
			v.fuel = b.GetFluidFraction(MotorbikeFluid.FUEL);
			v.ruined = b.IsRuined();
			v.engine = b.EngineIsOn();
			v.occupants = new array<string>;
			for (int i = 0; i < b.CrewSize(); i++)
			{
				PlayerBase crew = PlayerBase.Cast(b.CrewMember(i));
				if (crew && crew.GetIdentity())
					v.occupants.Insert(crew.GetIdentity().GetPlainId());
			}
			list.Insert(v);
		}
	}

	protected static void FillFluid(MotorbikeScript bike, MotorbikeFluid fluid)
	{
		float missing = bike.GetFluidCapacity(fluid) * (1 - bike.GetFluidFraction(fluid));
		if (missing > 0)
			bike.Fill(fluid, missing);
	}

	// Same scopes as for cars. A motorbike only has fuel (MotorbikeFluid has no oil, brake fluid or coolant).
	static string Repair(MotorbikeScript bike, string scope)
	{
		if (scope == "all" || scope == "engine")
			DZOAdminWorld.RepairEntity(bike);
		if (scope == "all" || scope == "parts" || scope == "wheels")
		{
			GameInventory inv = bike.GetInventory();
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
		{
			FillFluid(bike, MotorbikeFluid.FUEL);
		}
		return "";
	}
}

modded class MotorbikeScript
{
	override void EEInit()
	{
		super.EEInit();
		if (GetGame().IsServer())
		{
			DZOAdminBikes.s_Bikes.Insert(this);
			if (DZOAdminWatch.s_Rules)
				DZOAdminWatch.OnEntityInit(this);
		}
	}

	override void EEDelete(EntityAI parent)
	{
		DZOAdminBikes.s_Bikes.RemoveItem(this);
		DZOAdminWatch.OnEntityDelete(this);
		super.EEDelete(parent);
	}
}
#endif
