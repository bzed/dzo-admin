// SPDX-FileCopyrightText: 2026 Bernd Zeimetz <bernd@bzed.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

// An active sandstorm on the map. The game gives the storm's centre, heading, speed and the intensity at any
// point, but not its shape, so the area is sampled on a grid and sent as rectangles of two strengths on the layer
// "Sandstorm" (one rectangle per run of cells of a row), plus one marker at the centre. Everybody sees it.
class DZOAdminSandstorm
{
	static const string LAYER = "Sandstorm";
	static const string COLOR = "#c8a464";
	// The cells per side of the grid over the world, and the strength (of the storm's magnitude) from which a
	// cell is light and heavy.
	static const int GRID = 48;
	static const float BAND_LIGHT = 0.15;
	static const float BAND_HEAVY = 0.5;

	protected static void Prop(DZOAdminMarker m, string k, string v)
	{
		DZOAdminKV kv = new DZOAdminKV();
		kv.k = k;
		kv.v = v;
		m.props.Insert(kv);
	}

	protected static void Layer(array<ref DZOAdminLayer> layers)
	{
		foreach (DZOAdminLayer have : layers)
		{
			if (have.name == LAYER)
				return;
		}
		DZOAdminLayer l = new DZOAdminLayer();
		l.name = LAYER;
		l.icon = "asterisk";
		l.color = COLOR;
		l.visibility = "viewer";
		layers.Insert(l);
	}

	// The compass point and degrees of a direction on the map (0 = north, +z; 90 = east), "NE (47°)", or "" for
	// no direction.
	static string Heading(vector dir)
	{
		if (dir[0] == 0 && dir[2] == 0)
			return "";
		float deg = Math.Atan2(dir[0], dir[2]) * Math.RAD2DEG;
		if (deg < 0)
			deg += 360;
		TStringArray points = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
		int idx = Math.Round(deg / 45);
		idx = idx - (idx / 8) * 8;
		int d = Math.Round(deg);
		d = d - (d / 360) * 360;
		return points.Get(idx) + " (" + d.ToString() + "°)";
	}

	// One rectangle of the cells from..to of a row, as a polygon marker.
	protected static void Rect(array<ref DZOAdminMarker> result, string band, int row, int run, int from, int to, float cell)
	{
		float x0 = from * cell;
		float x1 = (to + 1) * cell;
		float z0 = row * cell;
		float z1 = (row + 1) * cell;
		DZOAdminMarker m = new DZOAdminMarker();
		m.layer = LAYER;
		m.id = "s" + band + row.ToString() + "_" + run.ToString();
		m.shape = "polygon";
		m.points = new array<ref DZOAdminPoint>;
		AddPoint(m, x0, z0);
		AddPoint(m, x1, z0);
		AddPoint(m, x1, z1);
		AddPoint(m, x0, z1);
		m.x = (x0 + x1) / 2;
		m.z = (z0 + z1) / 2;
		m.color = COLOR;
		m.props = new array<ref DZOAdminKV>;
		Prop(m, "band", band);
		m.visibility = "viewer";
		m.source = "sandstorm";
		result.Insert(m);
	}

	protected static void AddPoint(DZOAdminMarker m, float x, float z)
	{
		DZOAdminPoint p = new DZOAdminPoint();
		p.x = x;
		p.z = z;
		m.points.Insert(p);
	}

	// Walks the cells of a row and emits a rectangle for each run at or above a strength.
	protected static void Runs(array<ref DZOAdminMarker> result, string band, int row, array<float> level, float from, float cell)
	{
		int run = 0;
		int start = -1;
		for (int c = 0; c <= GRID; c++)
		{
			bool on = c < GRID && level.Get(c) >= from;
			if (on && start < 0)
				start = c;
			if (!on && start >= 0)
			{
				Rect(result, band, row, run, start, c - 1, cell);
				run++;
				start = -1;
			}
		}
	}

	static void Collect(array<ref DZOAdminMarker> result, array<ref DZOAdminLayer> layers)
	{
		Weather w = GetGame().GetWeather();
		if (!w || !w.GetSandstorm() || !w.GetSandstorm().IsActive())
			return;
		float magnitude = Math.Max(w.GetSandstorm().GetMagnitude(), 0.01);
		float size = GetGame().GetWorld().GetWorldSize();
		float cell = size / GRID;
		Layer(layers);
		array<float> level = new array<float>;
		for (int row = 0; row < GRID; row++)
		{
			level.Clear();
			for (int col = 0; col < GRID; col++)
				level.Insert(w.GetSandstorm().GetIntensity(Vector((col + 0.5) * cell, 0, (row + 0.5) * cell)) / magnitude);
			Runs(result, "light", row, level, BAND_LIGHT, cell);
			Runs(result, "heavy", row, level, BAND_HEAVY, cell);
		}

		vector p = w.GetSandstorm().GetPosition();
		DZOAdminMarker m = new DZOAdminMarker();
		m.layer = LAYER;
		m.id = "storm";
		m.shape = "point";
		m.x = p[0];
		m.y = p[1];
		m.z = p[2];
		m.icon = "asterisk";
		m.color = COLOR;
		m.label = "Sandstorm";
		m.props = new array<ref DZOAdminKV>;
		m.visibility = "viewer";
		m.source = "sandstorm";
		string heading = Heading(w.GetSandstorm().GetDirection());
		if (heading != "")
			Prop(m, "heading", heading);
		int kmh = Math.Round(w.GetSandstorm().GetSpeed() * 3.6);
		Prop(m, "speed", kmh.ToString() + " km/h");
		int strength = Math.Round(w.GetSandstorm().GetMagnitude() * 100);
		Prop(m, "strength", strength.ToString() + "%");
		float remaining = w.GetSandstorm().GetRemainingMovementDuration();
		if (w.GetSandstorm().IsPositionAtEnd() || remaining <= 0)
			Prop(m, "moving", "arrived");
		else
			m.ttl = Math.Ceil(remaining);
		result.Insert(m);
	}
}
