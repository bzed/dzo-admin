// SPDX-FileCopyrightText: 2026 Bernd Zeimetz <bernd@bzed.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

// A class watch rule (zero-code marker integration): every entity whose type
// is one of `classes` (or a subclass) becomes a marker on `layer`. A trailing
// `*` in a class name is a prefix glob.
class DZOAdminWatchRule
{
	string layer;
	ref array<string> classes;
	string icon;
	string label;
	int max;
}

// $profile:dzo/admin/config.json, written by `dzo instance render` (never in
// git: it holds the instance token).
class DZOAdminConfig
{
	int version;
	string endpoint;      // e.g. "http://127.0.0.1:2400/", trailing slash
	string token;
	int sync_ms = 1000;   // command poll / push tick
	int players_s = 5;
	int vehicles_s = 60;
	int markers_s = 10;
	int events_s = 30;
	bool allow_spawn = true;
	ref array<string> allow_classes; // empty = every public class
	ref array<string> deny_classes;
	ref array<ref DZOAdminWatchRule> watch;

	// Match reports whether `type` is selected by the glob list `list`. A pattern is a class name, in
	// which one `*` stands for any text: "Wreck_*", "*_DE", "StaticObj_Wreck_*_DE".
	static bool MatchList(array<string> list, string type)
	{
		if (!list)
			return false;
		foreach (string pattern : list)
		{
			int n = pattern.Length();
			if (n == 0)
				continue;
			int star = pattern.IndexOf("*");
			if (star < 0)
			{
				if (pattern == type)
					return true;
				continue;
			}
			string prefix = pattern.Substring(0, star);
			string suffix = pattern.Substring(star + 1, n - star - 1);
			int t = type.Length();
			// Substring throws when the length exceeds the string.
			if (t < prefix.Length() + suffix.Length())
				continue;
			if (prefix.Length() > 0 && type.Substring(0, prefix.Length()) != prefix)
				continue;
			if (suffix.Length() > 0 && type.Substring(t - suffix.Length(), suffix.Length()) != suffix)
				continue;
			return true;
		}
		return false;
	}

	bool SpawnAllowed(string type)
	{
		if (!allow_spawn)
			return false;
		if (MatchList(deny_classes, type))
			return false;
		if (allow_classes && allow_classes.Count() > 0)
			return MatchList(allow_classes, type);
		return true;
	}
}
