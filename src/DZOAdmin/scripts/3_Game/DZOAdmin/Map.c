// SPDX-FileCopyrightText: 2026 Bernd Zeimetz <bernd@bzed.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

// The marker API other mods call to put content on the dzo admin map. Calls
// only update an in-memory table; DZOAdminService publishes it in batches.
// Other mods wrap their calls in #ifdef DZO_ADMIN, so they load without
// dzo-admin (see examples/marker-demo).
class DZOAdmin_Map
{
	static const int API_VERSION = 1;
	static const int MAX_MARKERS = 2000;

	protected static ref map<string, ref DZOAdminMarker> s_Markers = new map<string, ref DZOAdminMarker>;
	protected static ref map<string, Object> s_Tracked = new map<string, Object>;
	protected static ref map<string, ref DZOAdminLayer> s_Layers = new map<string, ref DZOAdminLayer>;
	protected static ref map<string, float> s_Expires = new map<string, float>;

	protected static string Key(string layer, string id)
	{
		return layer + "/" + id;
	}

	static void DefineLayer(string name, string icon, string color, string visibility = "admin")
	{
		DZOAdminLayer l = new DZOAdminLayer();
		l.name = name;
		l.icon = icon;
		l.color = color;
		l.visibility = visibility;
		s_Layers.Set(name, l);
	}

	// Upsert places or moves a point marker. ttl is in seconds, 0 = no expiry.
	// props is an optional key/value map shown in the tooltip.
	static void Upsert(string layer, string id, vector pos, string icon = "", string label = "", map<string, string> props = null, int ttl = 0)
	{
		string key = Key(layer, id);
		DZOAdminMarker m;
		if (!s_Markers.Find(key, m))
		{
			if (s_Markers.Count() >= MAX_MARKERS)
				return;
			m = new DZOAdminMarker();
			m.layer = layer;
			m.id = id;
			m.shape = "point";
			m.visibility = "admin";
			m.source = "api";
			s_Markers.Set(key, m);
		}
		m.x = pos[0];
		m.y = pos[1];
		m.z = pos[2];
		m.icon = icon;
		m.label = label;
		m.props = new array<ref DZOAdminKV>;
		if (props)
		{
			foreach (string k, string v : props)
			{
				DZOAdminKV kv = new DZOAdminKV();
				kv.k = k;
				kv.v = v;
				m.props.Insert(kv);
			}
		}
		m.ttl = ttl;
		if (ttl > 0)
			s_Expires.Set(key, GetGame().GetTickTime() + ttl);
		else
			s_Expires.Remove(key);
	}

	// Circle turns a marker into a circle of the given radius in metres.
	static void Circle(string layer, string id, vector center, float radius, string label = "", string color = "")
	{
		Upsert(layer, id, center, "", label);
		DZOAdminMarker m;
		if (s_Markers.Find(Key(layer, id), m))
		{
			m.shape = "circle";
			m.radius = radius;
			m.color = color;
		}
	}

	// Path sets a polyline (closed = false) or polygon (closed = true).
	static void Path(string layer, string id, array<vector> points, bool closed, string label = "")
	{
		if (!points || points.Count() == 0)
			return;
		Upsert(layer, id, points.Get(0), "", label);
		DZOAdminMarker m;
		if (!s_Markers.Find(Key(layer, id), m))
			return;
		if (closed)
			m.shape = "polygon";
		else
			m.shape = "polyline";
		m.points = new array<ref DZOAdminPoint>;
		foreach (vector v : points)
		{
			DZOAdminPoint p = new DZOAdminPoint();
			p.x = v[0];
			p.z = v[2];
			m.points.Insert(p);
		}
	}

	// Track follows an entity: the marker moves with it and is removed when
	// the entity is deleted.
	static void Track(string layer, Object entity, string icon = "", string label = "", int ttl = 0)
	{
		if (!entity)
			return;
		string id = "e" + entity.GetID().ToString();
		Upsert(layer, id, entity.GetPosition(), icon, label, null, ttl);
		DZOAdminMarker m;
		if (s_Markers.Find(Key(layer, id), m))
			m.source = "tracked";
		s_Tracked.Set(Key(layer, id), entity);
	}

	static void Untrack(string layer, Object entity)
	{
		if (entity)
			Remove(layer, "e" + entity.GetID().ToString());
	}

	static void Remove(string layer, string id)
	{
		string key = Key(layer, id);
		s_Markers.Remove(key);
		s_Tracked.Remove(key);
		s_Expires.Remove(key);
	}

	static void ClearLayer(string layer)
	{
		array<string> doomed = new array<string>;
		foreach (string key, DZOAdminMarker m : s_Markers)
		{
			if (m.layer == layer)
				doomed.Insert(key);
		}
		foreach (string k : doomed)
		{
			s_Markers.Remove(k);
			s_Tracked.Remove(k);
			s_Expires.Remove(k);
		}
	}

	// OnEntityDeleted drops every marker that follows `entity`. The EntityAI
	// hook calls it only while something is tracked.
	static void OnEntityDeleted(Object entity)
	{
		if (s_Tracked.Count() == 0)
			return;
		array<string> doomed = new array<string>;
		foreach (string key, Object o : s_Tracked)
		{
			if (o == entity)
				doomed.Insert(key);
		}
		foreach (string k : doomed)
		{
			s_Markers.Remove(k);
			s_Tracked.Remove(k);
			s_Expires.Remove(k);
		}
	}

	// Snapshot refreshes tracked positions, drops expired markers and returns
	// the current table. It is called by the service at its marker interval.
	static void Snapshot(array<ref DZOAdminMarker> markers, array<ref DZOAdminLayer> layers)
	{
		float now = GetGame().GetTickTime();
		array<string> doomed = new array<string>;
		foreach (string key, float until : s_Expires)
		{
			if (until <= now)
				doomed.Insert(key);
		}
		foreach (string k : doomed)
		{
			s_Markers.Remove(k);
			s_Tracked.Remove(k);
			s_Expires.Remove(k);
		}
		foreach (string tkey, Object o : s_Tracked)
		{
			DZOAdminMarker t;
			if (o && s_Markers.Find(tkey, t))
			{
				vector p = o.GetPosition();
				t.x = p[0];
				t.y = p[1];
				t.z = p[2];
			}
		}
		foreach (string mkey, DZOAdminMarker m : s_Markers)
		{
			if (m.ttl > 0 && s_Expires.Contains(mkey))
				m.ttl = Math.Max(1, Math.Round(s_Expires.Get(mkey) - now));
			markers.Insert(m);
		}
		foreach (string lname, DZOAdminLayer l : s_Layers)
			layers.Insert(l);
	}
}
