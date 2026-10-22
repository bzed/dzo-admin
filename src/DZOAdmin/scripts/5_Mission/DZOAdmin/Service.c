// SPDX-FileCopyrightText: 2026 Bernd Zeimetz <bernd@bzed.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

// A class list sent to dzo in pieces: the list is built once, on first use, and chunks keep the requests
// small. Items are the classes dzo can give to a player, vehicles the ones it can spawn on the map.
class DZOAdminTypeFeed
{
	protected bool m_Vehicles;
	protected ref array<string> m_List;
	protected string m_Hash;
	protected int m_Sent;

	void DZOAdminTypeFeed(bool vehicles)
	{
		m_Vehicles = vehicles;
	}

	// Start again from the first chunk (dzo lost the list).
	void Restart()
	{
		m_Sent = 0;
	}

	// Next returns the next chunk of the list, or null when all of it was sent.
	DZOAdminTypes Next()
	{
		if (!m_List)
		{
			m_List = new array<string>;
			if (m_Vehicles)
			{
				DZOAdminWorld.SpawnableVehicles(m_List);
				m_List.Sort(false);
			}
			else
			{
				DZOAdminWorld.SpawnableTypes(m_List);
				m_List.Sort(false);
				DZOAdminKits.Load(m_List);
			}
			// A cheap fingerprint (class count : total characters): dzo
			// re-reads the list only when it changes.
			int chars = 0;
			foreach (string t : m_List)
				chars += t.Length();
			m_Hash = m_List.Count().ToString() + ":" + chars.ToString();
		}
		if (m_Sent >= m_List.Count())
			return null;
		DZOAdminTypes ty = new DZOAdminTypes();
		ty.hash = m_Hash;
		ty.offset = m_Sent;
		ty.total = m_List.Count();
		ty.names = new array<string>;
		int end = Math.Min(m_List.Count(), m_Sent + 400);
		for (int i = m_Sent; i < end; i++)
			ty.names.Insert(m_List.Get(i));
		m_Sent = end;
		return ty;
	}
}

// The mod's main loop. Every sync_ms it posts whatever state is due, plus the
// results of finished commands, to dzo and executes the commands in the reply.
// Without a config (no dzo/admin/config.json) it only logs and stays idle, so
// the mod can be loaded before dzo has rendered the instance.
class DZOAdminService
{
	protected static ref DZOAdminService s_Instance;
	protected static const int MAX_DONE = 256;

	protected ref DZOAdminConfig m_Config;
	protected ref DZOAdminTransport m_Transport;
	protected ref array<ref DZOAdminResult> m_Results = new array<ref DZOAdminResult>;
	// Results already sent, remembered so a repeated command id is answered
	// again instead of being executed twice.
	protected ref map<string, ref DZOAdminResult> m_Done = new map<string, ref DZOAdminResult>;
	protected ref array<string> m_DoneOrder = new array<string>;
	protected ref DZOAdminTypeFeed m_Items;
	protected ref DZOAdminTypeFeed m_Vehicles;
	protected int m_Seq;
	protected bool m_Hello = true;
	protected float m_LastPlayers = -1000;
	protected float m_LastVehicles = -1000;
	protected float m_LastMarkers = -1000;
	protected float m_LastEvents = -1000;

	static void Start()
	{
		if (s_Instance)
			return;
		s_Instance = new DZOAdminService();
		s_Instance.Init();
	}

	protected void Init()
	{
		DZOAdminVersion.LogLoaded();
		if (!FileExist(DZOADMIN_CONFIG_PATH))
		{
			Print("[DZOAdmin] no " + DZOADMIN_CONFIG_PATH + ", idle");
			return;
		}
		m_Config = new DZOAdminConfig();
		string err;
		if (!JsonFileLoader<DZOAdminConfig>.LoadFile(DZOADMIN_CONFIG_PATH, m_Config, err))
		{
			Print("[DZOAdmin] cannot read config: " + err);
			m_Config = null;
			return;
		}
		if (m_Config.endpoint == "" || m_Config.token == "")
		{
			Print("[DZOAdmin] config has no endpoint or token, idle");
			m_Config = null;
			return;
		}
		m_Items = new DZOAdminTypeFeed(false);
		m_Vehicles = new DZOAdminTypeFeed(true);
		DZOAdminWatch.SetRules(m_Config.watch);
		DZOAdminFeed.Enable();
		m_Transport = new DZOAdminTransport(m_Config.endpoint);
		int tick = Math.Max(250, m_Config.sync_ms);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.Tick, tick, true);
		Print("[DZOAdmin] syncing with " + m_Config.endpoint);
	}

	protected bool Due(float last, int seconds)
	{
		return seconds > 0 && GetGame().GetTickTime() - last >= seconds;
	}

	protected void Tick()
	{
		if (!m_Transport.Ready())
			return;
		// Commands that arrived with the last reply run before the next push,
		// so their results go out with it.
		while (m_Transport.m_Inbox.Count() > 0)
		{
			DZOAdminCommand c = m_Transport.m_Inbox.Get(0);
			m_Transport.m_Inbox.RemoveOrdered(0);
			Execute(c);
		}
		if (m_Transport.m_ResendTypes)
		{
			m_Transport.m_ResendTypes = false;
			m_Items.Restart();
			m_Vehicles.Restart();
		}
		if (m_Transport.m_WantHello)
		{
			m_Hello = true;
			m_Transport.m_WantHello = false;
			// dzo lost its state (a restart, a lost request): send the class list again from the start.
			m_Items.Restart();
			m_Vehicles.Restart();
		}
		// The events of the last request: gone for good when dzo answered it, otherwise sent again.
		if (m_Transport.m_LastOk)
			DZOAdminFeed.Ack();
		else
			DZOAdminFeed.Retry();
		float now = GetGame().GetTickTime();
		DZOAdminSync s = new DZOAdminSync();
		s.token = m_Config.token;
		s.protocol = DZOADMIN_PROTOCOL_VERSION;
		s.mod_version = DZOADMIN_VERSION;
		s.seq = m_Seq;
		s.hello = m_Hello;
		s.has = new array<string>;
		int year, month, day, hour, minute;
		GetGame().GetWorld().GetDate(year, month, day, hour, minute);
		s.clock = hour * 60 + minute;
		s.has.Insert("clock");
		if (m_Hello)
		{
			string world;
			GetGame().GetWorldName(world);
			s.world = world;
			s.features = new array<string>;
#ifdef EXPANSIONMODCORE
			s.features.Insert("expansion");
#endif
			string list = "";
			foreach (string feature : s.features)
				list = list + " " + feature;
			Print("[DZOAdmin] features:" + list);
		}
		if (m_Hello || Due(m_LastPlayers, m_Config.players_s))
		{
			s.has.Insert("players");
			s.players = new array<ref DZOAdminPlayer>;
			DZOAdminWorld.CollectPlayers(s.players);
			m_LastPlayers = now;
		}
		if (m_Hello || Due(m_LastVehicles, m_Config.vehicles_s))
		{
			s.has.Insert("vehicles");
			s.vehicles = new array<ref DZOAdminVehicle>;
			DZOAdminWorld.CollectVehicles(s.vehicles);
			m_LastVehicles = now;
		}
		if (m_Hello || Due(m_LastMarkers, m_Config.markers_s))
		{
			s.has.Insert("markers");
			s.markers = new array<ref DZOAdminMarker>;
			s.layers = new array<ref DZOAdminLayer>;
			DZOAdmin_Map.Snapshot(s.markers, s.layers);
			DZOAdminWatch.WatchMarkers(s.markers);
			DZOAdminFlags.Collect(s.markers, s.layers);
			DZOAdminRebuilt.Collect(s.markers, s.layers);
			DZOAdminSandstorm.Collect(s.markers, s.layers);
			m_LastMarkers = now;
		}
		if (m_Hello || Due(m_LastEvents, m_Config.events_s))
		{
			s.has.Insert("events");
			s.events = new array<ref DZOAdminEvent>;
			DZOAdminWorld.CollectEvents(s.events);
			m_LastEvents = now;
		}
		array<ref DZOAdminFeedEvent> events = DZOAdminFeed.Take();
		if (events.Count() > 0)
		{
			s.feed = events;
			s.has.Insert("feed");
		}
		s.types = m_Items.Next();
		s.vehicle_types = m_Vehicles.Next();
		if (m_Results.Count() > 0)
		{
			s.results = new array<ref DZOAdminResult>;
			foreach (DZOAdminResult r : m_Results)
				s.results.Insert(r);
		}
		m_Seq++;
		m_Hello = false;
		m_Transport.Send(s);
		// A failed send drops this batch: state is re-pushed on the next
		// interval, and dzo asks again for results it never saw (it
		// re-sends the command, which m_Done answers without re-executing).
		m_Results.Clear();
	}

	protected void Execute(DZOAdminCommand c)
	{
		DZOAdminResult prior;
		if (m_Done.Find(c.id, prior))
		{
			m_Results.Insert(prior);
			return;
		}
		string err;
		if (c.kind == "message")
			err = DZOAdminWorld.Message(c);
		else if (c.kind == "teleport")
			err = DZOAdminWorld.Teleport(c);
		else if (c.kind == "player_kill")
			err = DZOAdminWorld.PlayerKill(c);
		else if (c.kind == "player_heal")
			err = DZOAdminWorld.PlayerHeal(c);
		else if (c.kind == "player_fixgear")
			err = DZOAdminWorld.PlayerFixGear(c);
		else if (c.kind == "spawn_item")
			err = DZOAdminWorld.SpawnItem(c, m_Config);
		else if (c.kind == "spawn_vehicle")
			err = DZOAdminWorld.SpawnVehicle(c, m_Config);
		else if (c.kind == "vehicle_repair")
			err = DZOAdminWorld.VehicleRepair(c);
		else if (c.kind == "vehicle_flip")
			err = DZOAdminWorld.VehicleFlip(c);
		else if (c.kind == "vehicle_delete")
			err = DZOAdminWorld.VehicleDelete(c);
		else if (c.kind == "flag_raise")
			err = DZOAdminFlags.Raise(c);
		else
			err = "unknown command kind " + c.kind;
		DZOAdminResult r = new DZOAdminResult();
		r.id = c.id;
		r.ok = err == "";
		r.message = err;
		m_Results.Insert(r);
		m_Done.Set(c.id, r);
		m_DoneOrder.Insert(c.id);
		if (m_DoneOrder.Count() > MAX_DONE)
		{
			m_Done.Remove(m_DoneOrder.Get(0));
			m_DoneOrder.RemoveOrdered(0);
		}
		Print("[DZOAdmin] " + c.kind + " " + c.id + ": " + err);
	}
}

modded class MissionServer
{
	override void OnInit()
	{
		super.OnInit();
		DZOAdminService.Start();
	}

	override void InvokeOnConnect(PlayerBase player, PlayerIdentity identity)
	{
		super.InvokeOnConnect(player, identity);
		DZOAdminFeed.Joined(player, identity);
	}

	// The feed entry is made first: the game lets the player go in super, and the identity may be gone by then.
	override void PlayerDisconnected(PlayerBase player, PlayerIdentity identity, string uid)
	{
		DZOAdminFeed.Left(player, identity, uid);
		super.PlayerDisconnected(player, identity, uid);
	}
}
