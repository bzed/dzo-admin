// SPDX-FileCopyrightText: 2026 Bernd Zeimetz <bernd@bzed.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

// Registries kept up to date by hooks on entity creation and deletion, so the
// mod never scans the world: vehicles and effect areas. Watch rules live in
// 3_Game/Watch.c (EntityAI is an engine class and cannot be modded).
class DZOAdminRegistry
{
	static ref array<CarScript> s_Cars = new array<CarScript>;
	static ref array<EffectArea> s_Areas = new array<EffectArea>;
}

modded class CarScript
{
	override void EEInit()
	{
		super.EEInit();
		if (GetGame().IsServer())
		{
			DZOAdminRegistry.s_Cars.Insert(this);
			if (DZOAdminWatch.s_Rules)
				DZOAdminWatch.OnEntityInit(this);
		}
	}

	override void EEDelete(EntityAI parent)
	{
		DZOAdminRegistry.s_Cars.RemoveItem(this);
		DZOAdminWatch.OnEntityDelete(this);
		super.EEDelete(parent);
	}
}

// An area is registered when it is created, not when it goes live: a dynamic zone (a thrown M18
// smoke grenade) shows up on the map at once, with the radius it gets in DeferredInit.
modded class EffectArea
{
	protected void DZOAdminRegister()
	{
		if (GetGame().IsServer() && DZOAdminRegistry.s_Areas.Find(this) == -1)
			DZOAdminRegistry.s_Areas.Insert(this);
	}

	override void EEInit()
	{
		super.EEInit();
		DZOAdminRegister();
	}

	override void InitZoneServer()
	{
		super.InitZoneServer();
		DZOAdminRegister();
	}

	override void EEDelete(EntityAI parent)
	{
		DZOAdminRegistry.s_Areas.RemoveItem(this);
		super.EEDelete(parent);
	}
}

// Buildings: the objects the Central Economy places for its events (wrecks, crashed helicopters,
// containers, cisterns) are buildings and houses. Whatever a watch rule selects becomes a marker, so the
// events show up on the map by the class of their object. The map's own buildings cost one lookup of
// their type in a cache each.
modded class BuildingBase
{
	override void EEInit()
	{
		super.EEInit();
		if (DZOAdminWatch.s_Rules && GetGame().IsServer())
			DZOAdminWatch.OnEntityInit(this);
	}

	override void EEDelete(EntityAI parent)
	{
		if (GetGame().IsServer())
			DZOAdminWatch.OnEntityDelete(this);
		super.EEDelete(parent);
	}
}

// EntityAI is an engine class and cannot be modded, so watch rules and
// followed markers hook ItemBase (items, fireplaces, ...) and CarScript above.
// The hooks return immediately while no rule or tracked marker exists.
modded class ItemBase
{
	override void EEInit()
	{
		super.EEInit();
		if (DZOAdminWatch.s_Rules && GetGame().IsServer())
			DZOAdminWatch.OnEntityInit(this);
	}

	override void EEDelete(EntityAI parent)
	{
		if (GetGame().IsServer())
			DZOAdminWatch.OnEntityDelete(this);
		super.EEDelete(parent);
	}
}
