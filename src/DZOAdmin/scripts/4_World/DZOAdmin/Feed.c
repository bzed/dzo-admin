// SPDX-FileCopyrightText: 2026 Bernd Zeimetz <bernd@bzed.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

// The event feed: the mod buffers what happens in the game (kills, deaths, joins, building, ...) and sends it
// with the next sync. Every event gets a number (seq) from 1 per start of the server; dzo counts each number
// once, so a batch that is sent again after a lost reply is harmless. A batch is kept until dzo has
// answered the sync that carried it: Take() hands it out, Ack() forgets it, Retry() puts it back in front.
// While dzo cannot be reached the newest MAX_PENDING events are kept; the older ones are replaced by one
// `dropped` event with their count.
class DZOAdminFeed
{
	static const int MAX_PENDING = 256;

	protected static bool s_On;
	protected static int s_Seq;
	protected static int s_Dropped;
	protected static ref array<ref DZOAdminFeedEvent> s_Pending;
	protected static ref array<ref DZOAdminFeedEvent> s_InFlight;
	// The Steam ID by the id the game uses for the player (PlayerIdentity.GetId): a leaving player's
	// identity may be gone, and a body in the world only knows the cached id.
	protected static ref map<string, string> s_Plain;
	// Steam IDs of the players who are in the game, so a respawn is not reported as a new join.
	protected static ref set<string> s_Online;

	static void Enable()
	{
		s_On = true;
		s_Pending = new array<ref DZOAdminFeedEvent>;
		s_InFlight = new array<ref DZOAdminFeedEvent>;
		s_Plain = new map<string, string>;
		s_Online = new set<string>;
	}

	static bool On()
	{
		return s_On;
	}

	// Add queues an event and gives it its number.
	static void Add(DZOAdminFeedEvent e)
	{
		if (!s_On || !e)
			return;
		s_Seq++;
		e.seq = s_Seq;
		s_Pending.Insert(e);
		while (s_Pending.Count() > MAX_PENDING)
		{
			s_Pending.RemoveOrdered(0);
			s_Dropped++;
		}
	}

	// Take returns the events to send with the next sync, and keeps them until Ack. A `dropped` event
	// comes first; it has no number of its own (seq 0), which dzo never skips.
	static array<ref DZOAdminFeedEvent> Take()
	{
		array<ref DZOAdminFeedEvent> batch = new array<ref DZOAdminFeedEvent>;
		if (!s_On)
			return batch;
		if (s_Dropped > 0)
		{
			DZOAdminFeedEvent d = new DZOAdminFeedEvent();
			d.kind = "dropped";
			d.text = s_Dropped.ToString();
			batch.Insert(d);
			s_Dropped = 0;
		}
		foreach (DZOAdminFeedEvent e : s_Pending)
			batch.Insert(e);
		s_Pending.Clear();
		s_InFlight = batch;
		return batch;
	}

	// Ack forgets the batch dzo has received.
	static void Ack()
	{
		if (s_InFlight)
			s_InFlight = new array<ref DZOAdminFeedEvent>;
	}

	// Retry puts the batch back in front of what came in meanwhile (dzo did not answer).
	static void Retry()
	{
		if (!s_On || s_InFlight.Count() == 0)
			return;
		array<ref DZOAdminFeedEvent> back = new array<ref DZOAdminFeedEvent>;
		foreach (DZOAdminFeedEvent e : s_InFlight)
			back.Insert(e);
		foreach (DZOAdminFeedEvent p : s_Pending)
			back.Insert(p);
		s_Pending = back;
		s_InFlight = new array<ref DZOAdminFeedEvent>;
		while (s_Pending.Count() > MAX_PENDING)
		{
			s_Pending.RemoveOrdered(0);
			s_Dropped++;
		}
	}

	static void Pos(DZOAdminFeedEvent e, vector p)
	{
		e.has_pos = true;
		e.x = p[0];
		e.y = p[1];
		e.z = p[2];
	}

#ifdef EXPANSIONMODAI
	// "AI" plus the faction in parentheses ("AI (East)"), when the AI's group has one.
	protected static string AIName(eAIBase ai)
	{
		eAIGroup group = ai.GetGroup();
		if (group)
		{
			eAIFaction faction = group.GetFaction();
			if (faction)
			{
				string n = faction.GetDisplayName();
				if (n == "")
					n = faction.GetName();
				if (n != "")
					return "AI (" + n + ")";
			}
		}
		return "AI";
	}
#endif

	protected static void Who(PlayerBase p, out string steamId, out string name, out bool ai)
	{
		steamId = "";
		name = "";
		ai = false;
		if (!p)
			return;
#ifdef EXPANSIONMODAI
		eAIBase bot = eAIBase.Cast(p);
		if (bot)
		{
			name = AIName(bot);
			ai = true;
			return;
		}
#endif
		PlayerIdentity id = p.GetIdentity();
		if (id)
		{
			steamId = id.GetPlainId();
			name = id.GetName();
			return;
		}
		// A body whose player has left: the name and the game's id were cached.
		name = p.GetCachedName();
		s_Plain.Find(p.GetCachedID(), steamId);
	}

	// Actor fills the actor of an event with a player (or an Expansion AI), from the identity or, when the
	// player is gone, the cached id and name (like PluginAdminLog.GetPlayerPrefix).
	static void Actor(DZOAdminFeedEvent e, PlayerBase p)
	{
		Who(p, e.steam_id, e.name, e.ai);
	}

	static void Target(DZOAdminFeedEvent e, PlayerBase p)
	{
		Who(p, e.target_steam_id, e.target_name, e.target_ai);
	}

	static DZOAdminFeedEvent New(string kind)
	{
		DZOAdminFeedEvent e = new DZOAdminFeedEvent();
		e.kind = kind;
		return e;
	}

	// ---- joins and leaves ----

	static void Joined(PlayerBase player, PlayerIdentity identity)
	{
		if (!s_On || !player || !identity)
			return;
		string plain = identity.GetPlainId();
		s_Plain.Set(identity.GetId(), plain);
		// A respawn after a death calls this again for a player who never left.
		if (s_Online.Find(plain) != -1)
			return;
		s_Online.Insert(plain);
		DZOAdminFeedEvent e = New("connect");
		Actor(e, player);
		Pos(e, player.GetPosition());
		Add(e);
	}

	// Left is called before the game lets the player go: the identity may already be gone then, and the
	// body is handled right after.
	static void Left(PlayerBase player, PlayerIdentity identity, string uid)
	{
		if (!s_On || !player)
			return;
		DZOAdminFeedEvent e = New("disconnect");
		if (identity)
		{
			e.steam_id = identity.GetPlainId();
			e.name = identity.GetName();
		}
		else
		{
			e.name = player.GetCachedName();
			s_Plain.Find(uid, e.steam_id);
		}
		Pos(e, player.GetPosition());
		Add(e);
		s_Online.RemoveItem(e.steam_id);
	}

	// ---- kills and deaths ----

	protected static void Kill(DZOAdminFeedEvent e, PlayerBase killer, string weapon, float distance)
	{
		e.kind = "kill";
		Actor(e, killer);
		e.weapon = weapon;
		e.distance = distance;
	}

	// Killed classifies a death the way PluginAdminLog.PlayerKilled does. It runs before the player's own
	// EEKilled goes on, while the identity is still set.
	static void Killed(PlayerBase victim, Object killer)
	{
		if (!s_On || !victim)
			return;
		DZOAdminFeedEvent e = New("death");
		Target(e, victim);
		Pos(e, victim.GetPosition());
		PlayerBase shooter;
		EntityAI source = EntityAI.Cast(killer);
		if (source)
			shooter = PlayerBase.Cast(source.GetHierarchyParent());
		if (!shooter)
			shooter = PlayerBase.Cast(killer);

		if (!killer || killer == victim)
		{
			e.cause = "self";
		}
		else if (killer.IsWeapon() || killer.IsMeleeWeapon())
		{
			string weapon = killer.GetDisplayName();
			if (shooter && shooter != victim)
			{
				float dist = 0;
				if (!killer.IsMeleeWeapon() && !ExplosivesBase.Cast(killer))
					dist = vector.Distance(victim.GetPosition(), shooter.GetPosition());
				Kill(e, shooter, weapon, dist);
			}
			else if (ExplosivesBase.Cast(killer))
			{
				e.cause = "explosion";
				e.weapon = weapon;
				e.object = killer.GetType();
			}
			else
			{
				e.cause = "other";
				e.object = killer.GetType();
			}
		}
		else if (killer.IsTransport())
		{
			Transport car = Transport.Cast(killer);
			PlayerBase driver;
			if (car)
				driver = PlayerBase.Cast(car.CrewDriver());
			if (driver)
				Kill(e, driver, killer.GetDisplayName(), 0);
			else
			{
				e.cause = "vehicle";
				e.weapon = killer.GetDisplayName();
				e.object = killer.GetType();
			}
		}
		else if (source && source.IsZombie())
		{
			e.cause = "infected";
			e.object = killer.GetType();
		}
		else if (source && source.IsAnimal())
		{
			e.cause = "animal";
			e.object = killer.GetType();
		}
		else if (shooter && shooter != victim)
		{
			Kill(e, shooter, "fists", 0);
		}
		else
		{
			e.cause = "other";
			e.object = killer.GetType();
		}
		Add(e);
	}
}

modded class PlayerBase
{
	override void EEKilled(Object killer)
	{
		if (GetGame().IsServer())
			DZOAdminFeed.Killed(this, killer);
		super.EEKilled(killer);
	}
}
