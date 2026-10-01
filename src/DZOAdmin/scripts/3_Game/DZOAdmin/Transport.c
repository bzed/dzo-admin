// SPDX-FileCopyrightText: 2026 Bernd Zeimetz <bernd@bzed.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

// Callback for one POST to dzo's /mod/v1/sync. The mod only ever connects
// outbound: each sync pushes state and results and the reply carries the
// pending commands.
class DZOAdminSyncCallback : RestCallback
{
	protected DZOAdminTransport m_Transport;

	void DZOAdminSyncCallback(DZOAdminTransport t)
	{
		m_Transport = t;
	}

	override void OnSuccess(string data, int dataSize)
	{
		m_Transport.OnReply(data);
	}

	override void OnError(int errorCode)
	{
		m_Transport.OnFailure("error " + errorCode);
	}

	override void OnTimeout()
	{
		m_Transport.OnFailure("timeout");
	}
}

class DZOAdminTransport
{
	protected string m_Endpoint;
	protected RestContext m_Ctx;
	protected ref DZOAdminSyncCallback m_Callback;
	protected ref JsonSerializer m_Json = new JsonSerializer();
	protected bool m_InFlight;
	protected int m_Failures;
	protected float m_NextTry;

	// Pending command replies, filled by OnReply and drained by the service.
	ref array<ref DZOAdminCommand> m_Inbox = new array<ref DZOAdminCommand>;
	bool m_LastOk;

	void DZOAdminTransport(string endpoint)
	{
		m_Endpoint = endpoint;
		m_Callback = new DZOAdminSyncCallback(this);
	}

	bool Busy()
	{
		return m_InFlight;
	}

	// Backoff after failures: 1 s doubling up to 30 s, so a dead dzo does
	// not cost a request per tick.
	bool Ready()
	{
		return !m_InFlight && GetGame().GetTickTime() >= m_NextTry;
	}

	void Send(DZOAdminSync sync)
	{
		if (m_InFlight)
			return;
		if (!m_Ctx)
		{
			m_Ctx = GetRestApi().GetRestContext(m_Endpoint);
			if (!m_Ctx)
			{
				OnFailure("no rest context");
				return;
			}
			m_Ctx.SetHeader("application/json");
		}
		string body;
		m_Json.WriteToString(sync, false, body);
		m_InFlight = true;
		m_Ctx.POST(m_Callback, "mod/v1/sync", body);
	}

	void OnReply(string data)
	{
		m_InFlight = false;
		DZOAdminReply reply = new DZOAdminReply();
		string err;
		if (!m_Json.ReadFromString(reply, data, err) || !reply.ok)
		{
			string msg = err;
			if (reply.error != "")
				msg = reply.error;
			OnFailure("bad reply: " + msg);
			return;
		}
		if (m_Failures > 0)
			Print("[DZOAdmin] dzo reachable again");
		m_Failures = 0;
		m_NextTry = 0;
		m_LastOk = true;
		if (reply.commands)
		{
			foreach (DZOAdminCommand c : reply.commands)
				m_Inbox.Insert(c);
		}
	}

	void OnFailure(string why)
	{
		m_InFlight = false;
		m_LastOk = false;
		m_Failures++;
		float wait = Math.Min(30, Math.Pow(2, m_Failures - 1));
		m_NextTry = GetGame().GetTickTime() + wait;
		if (m_Failures == 1 || m_Failures % 20 == 0)
			Print("[DZOAdmin] sync failed (" + why + "), retry in " + wait + " s");
	}
}
