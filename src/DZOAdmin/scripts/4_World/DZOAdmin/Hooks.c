// SPDX-FileCopyrightText: 2026 Bernd Zeimetz <bernd@bzed.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

// Registries kept up to date by hooks on entity creation and deletion, so the
// mod never scans the world: vehicles (for the vehicle state and actions),
// markers that follow an entity, class watch rules and effect areas.
class DZOAdminRegistry
{
	static ref array<CarScript> s_Cars = new array<CarScript>;
	static ref array<EffectArea> s_Areas = new array<EffectArea>;
	// Watched entities by layer, with the rule's max count enforced.
	static ref map<string, ref array<EntityAI>> s_Watched = new map<string, ref array<EntityAI>>;
	// type name -> index of the matching rule, -1 = none (cache, so only the
	// first entity of every class costs a rule evaluation).
	static ref map<string, int> s_TypeRule = new map<string, int>;
	static ref array<ref DZOAdminWatchRule> s_Rules;

	static void SetRules(array<ref DZOAdminWatchRule> rules)
	{
		s_Rules = rules;
		s_TypeRule.Clear();
	}

	protected static int RuleFor(string type)
	{
		int idx;
		if (s_TypeRule.Find(type, idx))
			return idx;
		idx = -1;
		if (s_Rules)
		{
			for (int i = 0; i < s_Rules.Count(); i++)
			{
				DZOAdminWatchRule r = s_Rules.Get(i);
				// Subclasses match through the config inheritance.
				if (DZOAdminConfig.MatchList(r.classes, type) || InheritsFromAny(type, r.classes))
				{
					idx = i;
					break;
				}
			}
		}
		s_TypeRule.Set(type, idx);
		return idx;
	}

	protected static bool InheritsFromAny(string type, array<string> classes)
	{
		if (!classes)
			return false;
		foreach (string c : classes)
		{
			int n = c.Length();
			if (n == 0 || c.Substring(n - 1, 1) == "*")
				continue;
			if (GetGame().IsKindOf(type, c))
				return true;
		}
		return false;
	}

	static void OnEntityInit(EntityAI e)
	{
		if (!s_Rules || s_Rules.Count() == 0)
			return;
		int idx = RuleFor(e.GetType());
		if (idx < 0)
			return;
		DZOAdminWatchRule r = s_Rules.Get(idx);
		array<EntityAI> list;
		if (!s_Watched.Find(r.layer, list))
		{
			list = new array<EntityAI>;
			s_Watched.Set(r.layer, list);
		}
		if (r.max > 0 && list.Count() >= r.max)
			return;
		list.Insert(e);
	}

	static void OnEntityDelete(EntityAI e)
	{
		DZOAdmin_Map.OnEntityDeleted(e);
		if (s_Watched.Count() == 0)
			return;
		foreach (string layer, array<EntityAI> list : s_Watched)
			list.RemoveItem(e);
	}

	// WatchMarkers appends one marker per watched entity.
	static void WatchMarkers(array<ref DZOAdminMarker> out)
	{
		if (!s_Rules)
			return;
		foreach (string layer, array<EntityAI> list : s_Watched)
		{
			DZOAdminWatchRule rule = null;
			foreach (DZOAdminWatchRule r : s_Rules)
			{
				if (r.layer == layer)
				{
					rule = r;
					break;
				}
			}
			if (!rule)
				continue;
			foreach (EntityAI e : list)
			{
				if (!e)
					continue;
				DZOAdminMarker m = new DZOAdminMarker();
				m.layer = layer;
				m.id = "w" + e.GetID();
				m.shape = "point";
				vector p = e.GetPosition();
				m.x = p[0];
				m.y = p[1];
				m.z = p[2];
				m.icon = rule.icon;
				m.label = rule.label;
				if (m.label == "")
					m.label = e.GetType();
				m.visibility = "admin";
				m.source = "watch";
				out.Insert(m);
			}
		}
	}
}

modded class CarScript
{
	override void EEInit()
	{
		super.EEInit();
		if (GetGame().IsServer())
			DZOAdminRegistry.s_Cars.Insert(this);
	}

	override void EEDelete(EntityAI parent)
	{
		DZOAdminRegistry.s_Cars.RemoveItem(this);
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

// Watch rules and followed markers need entity creation and deletion. The
// hooks return immediately while no rule or tracked marker exists.
modded class EntityAI
{
	override void EEInit()
	{
		super.EEInit();
		if (DZOAdminRegistry.s_Rules && GetGame().IsServer())
			DZOAdminRegistry.OnEntityInit(this);
	}

	override void EEDelete(EntityAI parent)
	{
		if (GetGame().IsServer())
			DZOAdminRegistry.OnEntityDelete(this);
		super.EEDelete(parent);
	}
}
