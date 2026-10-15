// SPDX-FileCopyrightText: 2026 Bernd Zeimetz <bernd@bzed.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

// Central Economy events put plain engine objects into the world (police car wrecks, supply crates,
// convoy wrecks, ...) that have no script class, so their creation cannot be hooked. dzo hands the mod
// the places each event can spawn at and the classes it spawns (config.json, event_scan); the mod
// looks at a few of those places per tick, round robin, and remembers where it found the objects. The
// events reported are the places where one is found right now; a place is looked at again every
// round, so an event that despawned disappears from the list after at most one round.
class DZOAdminEventScan
{

	protected static ref array<ref DZOAdminScanRule> s_Rules;
	// One entry per place, in the order of the rules: the class found there, "" for none.
	protected static ref array<string> s_Found = new array<string>;
	protected static ref array<int> s_Rule = new array<int>;
	protected static ref array<int> s_Spawn = new array<int>;
	protected static int s_Cursor;
	protected static float s_LastStep;
	protected static float s_Budget;
	protected static ref array<Object> s_Objects = new array<Object>;
	protected static ref array<CargoBase> s_Proxy = new array<CargoBase>;

	static void SetRules(array<ref DZOAdminScanRule> rules)
	{
		s_Rules = rules;
		s_Found.Clear();
		s_Rule.Clear();
		s_Spawn.Clear();
		s_Cursor = 0;
		if (!rules)
			return;
		for (int r = 0; r < rules.Count(); r++)
		{
			DZOAdminScanRule rule = rules.Get(r);
			if (!rule || !rule.spawns)
				continue;
			for (int s = 0; s < rule.spawns.Count(); s++)
			{
				s_Rule.Insert(r);
				s_Spawn.Insert(s);
				s_Found.Insert("");
			}
		}
		Print("[DZOAdmin] looking for the objects of " + rules.Count() + " events at " + s_Found.Count() + " places");
	}

	// The classes looked for at one place: the rule's own, or those of the group of that place.
	protected static array<string> ClassesAt(DZOAdminScanRule rule, DZOAdminScanSpawn sp, out float radius)
	{
		radius = rule.radius;
		if (sp.g != "" && rule.groups)
		{
			foreach (DZOAdminScanGroup g : rule.groups)
			{
				if (g && g.name == sp.g)
				{
					radius = g.radius;
					return g.classes;
				}
			}
			return null;
		}
		return rule.classes;
	}

	protected static string Look(DZOAdminScanRule rule, DZOAdminScanSpawn sp)
	{
		float radius;
		array<string> classes = ClassesAt(rule, sp, radius);
		if (!classes || classes.Count() == 0)
			return "";
		float y = sp.y;
		if (y == 0)
			y = GetGame().SurfaceY(sp.x, sp.z);
		s_Objects.Clear();
		s_Proxy.Clear();
		GetGame().GetObjectsAtPosition3D(Vector(sp.x, y, sp.z), radius, s_Objects, s_Proxy);
		foreach (Object o : s_Objects)
		{
			if (!o)
				continue;
			foreach (string c : classes)
			{
				if (o.IsKindOf(c))
					return o.GetType();
			}
		}
		return "";
	}

	// Looks at the next places; called every sync tick. The places are spread over the time of a round
	// (event_scan_s), so a map with many places is not looked at faster than one with few.
	static void Step(int roundSeconds)
	{
		int n = s_Found.Count();
		if (n == 0)
			return;
		float now = GetGame().GetTickTime();
		if (s_LastStep == 0)
			s_LastStep = now;
		s_Budget += n * (now - s_LastStep) / Math.Max(roundSeconds, 1);
		s_LastStep = now;
		while (s_Budget >= 1)
		{
			s_Budget -= 1;
			if (s_Cursor >= n)
				s_Cursor = 0;
			DZOAdminScanRule rule = s_Rules.Get(s_Rule.Get(s_Cursor));
			s_Found.Set(s_Cursor, Look(rule, rule.spawns.Get(s_Spawn.Get(s_Cursor))));
			s_Cursor++;
		}
	}

	static void Collect(array<ref DZOAdminEvent> list)
	{
		for (int i = 0; i < s_Found.Count(); i++)
		{
			if (s_Found.Get(i) == "")
				continue;
			DZOAdminScanRule rule = s_Rules.Get(s_Rule.Get(i));
			DZOAdminScanSpawn sp = rule.spawns.Get(s_Spawn.Get(i));
			DZOAdminEvent e = new DZOAdminEvent();
			e.id = "ce" + i;
			e.name = s_Found.Get(i);
			e.category = rule.name;
			e.x = sp.x;
			e.y = sp.y;
			e.z = sp.z;
			e.source = "event_scan";
			list.Insert(e);
		}
	}
}
