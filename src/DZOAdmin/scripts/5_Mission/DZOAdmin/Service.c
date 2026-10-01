// SPDX-FileCopyrightText: 2026 Bernd Zeimetz <bernd@bzed.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

// The mod's main loop. Every sync_ms it posts whatever state is due, plus the
// results of finished commands, to dzo and executes the commands in the reply.
// Without a config (no dzo-admin/config.json) it only logs and stays idle, so
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
	protected ref array<string> m_Types;
	protected string m_TypesHash;
	protected int m_TypesSent;
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
		DZOAdminRegistry.SetRules(m_Config.watch);
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
		float now = GetGame().GetTickTime();
		DZOAdminSync s = new DZOAdminSync();
		s.token = m_Config.token;
		s.proto = DZOADMIN_PROTOCOL_VERSION;
		s.mod_version = DZOADMIN_VERSION;
		s.seq = m_Seq;
		s.hello = m_Hello;
		if (m_Hello)
		{
			string world;
			GetGame().GetWorldName(world);
			s.world = world;
		}
		if (m_Hello || Due(m_LastPlayers, m_Config.players_s))
		{
			s.players = new array<ref DZOAdminPlayer>;
			DZOAdminWorld.CollectPlayers(s.players);
			m_LastPlayers = now;
		}
		if (m_Hello || Due(m_LastVehicles, m_Config.vehicles_s))
		{
			s.vehicles = new array<ref DZOAdminVehicle>;
			DZOAdminWorld.CollectVehicles(s.vehicles);
			m_LastVehicles = now;
		}
		if (m_Hello || Due(m_LastMarkers, m_Config.markers_s))
		{
			s.markers = new array<ref DZOAdminMarker>;
			s.layers = new array<ref DZOAdminLayer>;
			DZOAdmin_Map.Snapshot(s.markers, s.layers);
			DZOAdminRegistry.WatchMarkers(s.markers);
			m_LastMarkers = now;
		}
		if (m_Hello || Due(m_LastEvents, m_Config.events_s))
		{
			s.events = new array<ref DZOAdminEvent>;
			DZOAdminWorld.CollectEvents(s.events);
			m_LastEvents = now;
		}
		s.types = NextTypes();
		if (m_Results.Count() > 0)
		{
			s.results = new array<ref DZOAdminResult>;
			s.results.Copy(m_Results);
		}
		m_Seq++;
		m_Hello = false;
		m_Transport.Send(s);
		// A failed send drops this batch: state is re-pushed on the next
		// interval, and dzo asks again for results it never saw (it
		// re-sends the command, which m_Done answers without re-executing).
		m_Results.Clear();
	}

	// NextTypes returns the next chunk of the spawnable class list (the list
	// is built once, on first use, and sent in pieces to keep requests small).
	protected DZOAdminTypes NextTypes()
	{
		if (!m_Types)
		{
			m_Types = new array<string>;
			DZOAdminWorld.SpawnableTypes(m_Types);
			m_Types.Sort(false);
			// A cheap fingerprint (class count : total characters): dzo
			// re-reads the list only when it changes.
			int chars = 0;
			foreach (string t : m_Types)
				chars += t.Length();
			m_TypesHash = m_Types.Count().ToString() + ":" + chars.ToString();
		}
		if (m_TypesSent >= m_Types.Count())
			return null;
		DZOAdminTypes ty = new DZOAdminTypes();
		ty.hash = m_TypesHash;
		ty.offset = m_TypesSent;
		ty.total = m_Types.Count();
		ty.names = new array<string>;
		int end = Math.Min(m_Types.Count(), m_TypesSent + 400);
		for (int i = m_TypesSent; i < end; i++)
			ty.names.Insert(m_Types.Get(i));
		m_TypesSent = end;
		return ty;
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
		else if (c.kind == "spawn_item")
			err = DZOAdminWorld.SpawnItem(c, m_Config);
		else if (c.kind == "vehicle_repair")
			err = DZOAdminWorld.VehicleRepair(c);
		else if (c.kind == "vehicle_delete")
			err = DZOAdminWorld.VehicleDelete(c);
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
}
