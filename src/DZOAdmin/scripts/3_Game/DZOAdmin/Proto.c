// SPDX-FileCopyrightText: 2026 Bernd Zeimetz <bernd@bzed.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

// Wire types of the dzo <-> dzo-admin protocol. They are serialised with
// JsonSerializer, so every public field is a JSON key with the same name.
// Positions are plain x/y/z floats (no vector) to keep the JSON flat. The Go
// side (internal/admin/proto.go) mirrors these classes field by field.

class DZOAdminPlayer
{
	string steam_id;
	string name;
	int slot;
	int ping;
	float x;
	float y;
	float z;
	float yaw;
	float health;
	float blood;
	float shock;
	bool alive;
	bool unconscious;
}

class DZOAdminVehicle
{
	string id;
	string kind;       // car, motorbike
	string type;
	float x;
	float y;
	float z;
	float yaw;
	float health;
	float fuel;
	bool ruined;
	bool engine;
	ref array<string> occupants;
	string owner;      // DayZ Expansion: the name on the key chain, when it is loaded
	bool covered;      // DayZ Expansion: stored under a cover, only its class and place are known
}

class DZOAdminKV
{
	string k;
	string v;
}

class DZOAdminPoint
{
	float x;
	float z;
}

class DZOAdminMarker
{
	string layer;
	string id;
	string shape;
	float x;
	float y;
	float z;
	float radius;
	ref array<ref DZOAdminPoint> points;
	string icon;
	string color;
	string label;
	ref array<ref DZOAdminKV> props;
	int ttl;
	string visibility;
	string source;
}

class DZOAdminLayer
{
	string name;
	string icon;
	string color;
	string visibility;
}

class DZOAdminEvent
{
	string id;
	string name;
	string category;
	float x;
	float y;
	float z;
	float radius;
	string source;
}

class DZOAdminTypes
{
	string hash;
	int offset;
	int total;
	ref array<string> names;
}

class DZOAdminResult
{
	string id;
	bool ok;
	string message;
}

// One class for every command so the reply parses with a single type. dzo
// sends all fields; the ones a kind does not use stay at their zero value.
class DZOAdminCommand
{
	string id;
	string kind;       // message, teleport, spawn_item, spawn_vehicle, vehicle_repair, vehicle_flip, vehicle_delete
	string steam_id;   // target player, "" = everyone (message), the ground at x/z (spawn_item)
	string to_steam_id; // teleport to this player
	string text;
	string style;      // message: chat (yellow), action (the same), blue, important (red), friendly (green), notification
	string type;       // spawn_item, spawn_vehicle: class name
	float quantity;
	bool attachments;
	float health;      // 0..1, 0 = default
	string target;     // spawn_item: inventory, hands, ground
	float x;
	float y;
	float z;
	bool has_y;
	string vehicle;    // vehicle id
	string scope;      // vehicle_repair: all, engine, parts, wheels, fluids, missing
	bool force;        // vehicle_delete: even with crew inside
}

class DZOAdminSync
{
	string token;
	int protocol;
	string mod_version;
	string world;
	int seq;
	bool hello;
	// The in-game time of day in minutes since midnight; sent with every request, listed in `has` as "clock".
	int clock;
	// The JSON writer turns a null array into [], so the parts that are
	// really included are listed here: players, vehicles, markers, events.
	ref array<string> has;
	ref array<ref DZOAdminPlayer> players;
	ref array<ref DZOAdminVehicle> vehicles;
	ref array<ref DZOAdminMarker> markers;
	ref array<ref DZOAdminLayer> layers;
	ref array<ref DZOAdminEvent> events;
	ref DZOAdminTypes types;
	ref DZOAdminTypes vehicle_types;
	ref array<ref DZOAdminResult> results;
}

class DZOAdminReply
{
	bool ok;
	string error;
	int protocol;
	ref array<ref DZOAdminCommand> commands;
	// dzo asks for the hello (with the world name) again, e.g. after dzo serve restarted.
	bool hello;
	// dzo lost a part of the class list and wants it again from the start.
	bool resend_types;
}
