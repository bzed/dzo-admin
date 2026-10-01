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

modded class EffectArea
{
	override void InitZoneServer()
	{
		super.InitZoneServer();
		DZOAdminRegistry.s_Areas.Insert(this);
	}

	override void EEDelete(EntityAI parent)
	{
		DZOAdminRegistry.s_Areas.RemoveItem(this);
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
