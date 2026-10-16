// SPDX-FileCopyrightText: 2026 Bernd Zeimetz <bernd@bzed.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

// What an item needs to be usable, worked out from the item configs: for every class of the
// spawn list, the part that fits each of its attachment slots. The result is cached in
// $profile:dzo-admin/kits.json together with a fingerprint of the class list, so a restart only
// compares the fingerprint and scans again when the set of classes changed (a mod was added,
// updated or removed).

const string DZOADMIN_KITS_PATH = "$profile:dzo-admin/kits.json";

class DZOAdminKitCache
{
	string hash;
	// One row per class that has a kit: "Class=Part1,Part2".
	ref array<string> rows;
}

class DZOAdminKits
{
	protected static ref map<string, ref TStringArray> s_Kits;
	protected static ref array<string> s_Classes;
	protected static float s_Rebuilt;

	// Fingerprint of a class list: any added, removed or renamed class changes it.
	static string Fingerprint(array<string> classes)
	{
		int h = 0;
		foreach (string c : classes)
			h = (h * 31 + c.Hash()) % 16777213;
		return classes.Count().ToString() + ":" + h.ToString();
	}

	// Load builds the kit table for the class list: from the cache file when its fingerprint
	// matches, by scanning the configs otherwise (and then writes the file).
	static void Load(array<string> classes)
	{
		s_Classes = classes;
		s_Kits = new map<string, ref TStringArray>;
		string hash = Fingerprint(classes);
		if (FileExist(DZOADMIN_KITS_PATH))
		{
			DZOAdminKitCache cache = new DZOAdminKitCache();
			string err;
			if (JsonFileLoader<DZOAdminKitCache>.LoadFile(DZOADMIN_KITS_PATH, cache, err) && cache.hash == hash && cache.rows)
			{
				foreach (string row : cache.rows)
				{
					TStringArray halves = new TStringArray;
					row.Split("=", halves);
					if (halves.Count() != 2)
						continue;
					TStringArray parts = new TStringArray;
					halves.Get(1).Split(",", parts);
					s_Kits.Insert(halves.Get(0), parts);
				}
				Print("[DZOAdmin] kits: " + s_Kits.Count().ToString() + " classes from the cache");
				return;
			}
		}
		Rebuild(classes, hash);
		Print("[DZOAdmin] kits: scanned " + classes.Count().ToString() + " classes, " + s_Kits.Count().ToString() + " have a kit");
	}

	// Rebuild scans the configs and writes the cache file.
	protected static void Rebuild(array<string> classes, string hash)
	{
		s_Classes = classes;
		s_Kits = new map<string, ref TStringArray>;
		Scan(classes);
		DZOAdminKitCache fresh = new DZOAdminKitCache();
		fresh.hash = hash;
		fresh.rows = new array<string>;
		for (int i = 0; i < s_Kits.Count(); i++)
		{
			string cls = s_Kits.GetKey(i);
			string joined;
			TStringArray ps = s_Kits.GetElement(i);
			joined = "";
			foreach (string p : ps)
			{
				if (joined != "")
					joined = joined + ",";
				joined = joined + p;
			}
			fresh.rows.Insert(cls + "=" + joined);
		}
		JsonFileLoader<DZOAdminKitCache>.JsonSaveFile(DZOADMIN_KITS_PATH, fresh);
	}

	// Kit returns the parts for a class, or null.
	static TStringArray Kit(string cls)
	{
		if (!s_Kits)
			return null;
		TStringArray k;
		if (s_Kits.Find(cls, k))
			return k;
		return null;
	}

	protected static string ConfigPath(string cls)
	{
		if (GetGame().ConfigIsExisting("CfgVehicles " + cls))
			return "CfgVehicles " + cls;
		if (GetGame().ConfigIsExisting("CfgWeapons " + cls))
			return "CfgWeapons " + cls;
		return "CfgMagazines " + cls;
	}

	// A config value that is a list in some classes and a single string in others.
	protected static void Strings(string path, TStringArray into)
	{
		GetGame().ConfigGetTextArray(path, into);
		if (into.Count() == 0)
		{
			string one;
			GetGame().ConfigGetText(path, one);
			if (one != "")
				into.Insert(one);
		}
	}

	protected static void Scan(array<string> classes)
	{
		// slot name -> classes that fit it, in config order
		map<string, ref TStringArray> bySlot = new map<string, ref TStringArray>;
		foreach (string cls : classes)
		{
			TStringArray slots = new TStringArray;
			Strings(ConfigPath(cls) + " inventorySlot", slots);
			foreach (string slot : slots)
			{
				TStringArray fit;
				if (!bySlot.Find(slot, fit))
				{
					fit = new TStringArray;
					bySlot.Insert(slot, fit);
				}
				fit.Insert(cls);
			}
		}
		foreach (string host : classes)
		{
			TStringArray wanted = new TStringArray;
			GetGame().ConfigGetTextArray(ConfigPath(host) + " attachments", wanted);
			TStringArray kit = new TStringArray;
			foreach (string w : wanted)
			{
				TStringArray cand;
				if (!bySlot.Find(w, cand))
					continue;
				string pick = Choose(host, cand);
				if (pick != "" && pick != host && kit.Find(pick) < 0)
					kit.Insert(pick);
			}
			if (kit.Count() > 0)
				s_Kits.Insert(host, kit);
		}
	}

	// The text from the last "_" to the end ("_Black"), or "".
	protected static string UnderscoreSuffix(string name)
	{
		int at = name.LastIndexOf("_");
		if (at < 0 || at == name.Length() - 1)
			return "";
		return name.Substring(at, name.Length() - at);
	}

	// The last capitalised word at the end ("Green" of "BackpackGreen"), or "".
	protected static string WordSuffix(string name)
	{
		string low = "abcdefghijklmnopqrstuvwxyz0123456789";
		string up = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
		int i = name.Length() - 1;
		while (i >= 0 && low.Contains(name.Get(i)))
			i--;
		if (i < 0 || i == name.Length() - 1 || !up.Contains(name.Get(i)))
			return "";
		return name.Substring(i, name.Length() - i);
	}

	// Several parts fit one slot: take the one that ends like the item ("_Black"), else the one with
	// the same last word ("Green"), else the first.
	static string Choose(string host, TStringArray cand)
	{
		if (cand.Count() == 0)
			return "";
		string s1 = UnderscoreSuffix(host);
		if (s1 != "")
		{
			foreach (string c1 : cand)
			{
				if (UnderscoreSuffix(c1) == s1)
					return c1;
			}
		}
		string s2 = WordSuffix(host);
		if (s2 != "")
		{
			foreach (string c2 : cand)
			{
				if (WordSuffix(c2) == s2)
					return c2;
			}
		}
		return cand.Get(0);
	}

	// Fit creates the parts of the kit on an item, and the parts' own kits, a few levels deep. A part
	// that no longer fits its slot means the cache is out of date (a mod changed without the class list
	// changing): the kits are scanned again, at most once in ten minutes.
	static void Fit(EntityAI item, int depth)
	{
		if (!FitParts(item, depth) && s_Classes && GetGame().GetTickTime() - s_Rebuilt > 600)
		{
			s_Rebuilt = GetGame().GetTickTime();
			Print("[DZOAdmin] kits: a part did not fit, scanning the configs again");
			Rebuild(s_Classes, Fingerprint(s_Classes));
		}
	}

	protected static bool FitParts(EntityAI item, int depth)
	{
		if (depth > 3 || !item)
			return true;
		TStringArray kit = Kit(item.GetType());
		if (!kit)
			return true;
		bool ok = true;
		foreach (string part : kit)
		{
			EntityAI made = item.GetInventory().CreateAttachment(part);
			if (!made)
				ok = false;
			else if (!FitParts(made, depth + 1))
				ok = false;
		}
		return ok;
	}
}
