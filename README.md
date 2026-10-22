<!--
SPDX-FileCopyrightText: 2026 Bernd Zeimetz <bernd@bzed.de>
SPDX-License-Identifier: AGPL-3.0-or-later
-->

# dzo-admin

Server-side DayZ admin mod for [dayz-server-operator](https://github.com/bzed/dayz-server-operator)
(`dzo`). It is a **servermod** (never sent to clients, no signing needed), and
is built into a PBO by the `dzo` repository, which carries this repository as
a submodule at `servermods/dzo-admin`.

Layout: `src/DZOAdmin/` is the PBO root (`$PBOPREFIX$` = `DZOAdmin`).

| Script module | File | Purpose |
|---|---|---|
| `3_Game` | `Proto.c` | wire types (JSON) |
| | `Config.c` | `$profile:dzo/admin/config.json` |
| | `Transport.c` | outbound `RestApi` POST with backoff |
| | `Map.c` | `DZOAdmin_Map`, the marker API for other mods |
| `4_World` | `Hooks.c` | vehicle/effect-area registries, class watch rules |
| | `Flags.c` | territory flags and Expansion territories as markers |
| | `Rebuilt.c` | rebuilt houses and wells (layers `Rebuilt houses`, `Rebuilt wells`, with the decay time as ttl) and the tunnel entrances (layer `Tunnel entrances`, another colour with a usable ladder) |
| | `Sandstorm.c` | an active sandstorm, sampled as light and heavy rectangles on the layer `Sandstorm` plus a centre marker |
| | `Feed.c` | the event feed: buffer, kills and deaths, joins and leaves |
| | `FeedHooks.c` | the event feed: base building, raids, flags, stashes |
| | `World.c` | state readers and the admin actions |
| `5_Mission` | `Service.c` | the sync loop, started from `MissionServer.OnInit` |

## Protocol (version 1)

The mod only connects outbound. Every `sync_ms` it sends
`POST <endpoint>mod/v1/sync` with a JSON body (the engine's `RestContext` can
only set `Content-Type`, so the instance token is a body field, not a header):

```json
{"token": "…", "protocol": 1, "mod_version": "0.1.0", "seq": 7, "hello": 0, "has": ["players", "vehicles"],
 "players": [...], "vehicles": [...], "markers": [...], "layers": [...],
 "events": [...], "types": {"hash": "…", "offset": 0, "total": 5000, "names": [...]},
 "results": [{"id": "c1", "ok": true, "message": ""}]}
```

State fields are only present when due (players every `players_s`, vehicles
every `vehicles_s`, markers every `markers_s`, events every `events_s`); the
first sync after a start has `"hello": true` and carries everything. The reply
is `{"ok": true, "protocol": 1, "commands": [...]}`. Commands: `message`,
`teleport`, `spawn_item`, `spawn_vehicle`, `vehicle_repair`, `vehicle_flip`, `vehicle_delete`, `flag_raise`; the field list is
`DZOAdminCommand` in `Proto.c`. A command id the mod has already executed is
answered from memory, not executed again.

## Event feed

The mod reports what happens in the game as an optional part `feed` of the sync (`has` lists `"feed"`; the
protocol stays 1). Events are buffered and go out with the next sync, numbered by `seq` (from 1 for each start of
the server), so a batch that is sent again because the reply was lost is counted once by dzo. At most 256
events wait while dzo cannot be reached; older ones are replaced by one `dropped` event with their count.
Every event has the fields of `DZOAdminFeedEvent` (`Proto.c`); the ones a kind does not use stay empty:

| kind | actor (`steam_id`, `name`, `ai`) | other fields |
|---|---|---|
| `kill` | the killer, an Expansion AI has `ai` and the name `AI (faction)` | target = the victim (`target_*`), `weapon` (display name; `fists`; the vehicle for a road kill), `distance` in metres (0 for melee and vehicles), position of the victim |
| `death` | none | target = the victim, `cause`: `self` (starved, bled, drowned, suicide), `infected`, `animal`, `explosion`, `vehicle` (no driver), `other`; `object` = class of the killer object |
| `connect`, `disconnect` | the player, position of the player | |
| `build`, `dismantle`, `destroy` | the player (none when nothing player-driven destroyed the part) | `object` = part, `text` = class of the base object, position of the object. A raid is one `destroy`; the parts that fall with it are not reported |
| `lock_cut` | the player | `weapon` = tool, `text` = the fence. The vanilla action is deprecated in 1.30 and no item offers it; mods that bring it back are reported |
| `flag_raised`, `flag_lowered` | the player | `object` = the flag pole class, `text` = the flag cloth class. dzo's own `flag_raise` is in dzo's audit log instead |
| `stash_buried`, `stash_dug_up` | the player | `object` = the stashed item's class |
| `dropped` | | `text` = number of events lost |

Chat is not reported by the mod: the 1.30 server scripts get no chat event. dzo reads it from BattlEye RCon.

## Map markers for other mods

See `examples/marker-demo`. Three ways: class watch rules in the config (no
code), the `DZOAdmin_Map` API behind `#ifdef DZO_ADMIN`, or JSON files in
`$profile:dzo/admin/markers/` (read by dzo directly).

## Status

Tested on DayZ 1.29 (stable) and 1.30 (experimental) against the real dzo
endpoint, with a real DayZ client (`scripts/test-client.sh` in the dzo
repository): it compiles, loads and syncs; the players state, messages in all
three styles, teleport, `spawn_item` (inventory, hands, ground), the refusals,
class watch rules on items, the crew of a vehicle, and vehicle repair/delete
work. On 1.30 motorbikes (`MotorbikeScript`, which is not a `CarScript`) are reported as vehicles
of `kind` `"motorbike"` (cars are `"car"`) and can be repaired (fuel only) and deleted; that part
is left out on 1.29, which has no motorbikes, and was verified on 1.30 only. See the dzo README,
"Status".

Enforce Script reminders from the boot: `proto` and `out` are reserved words
(even as field or parameter names), `EntityAI` is an engine class and cannot be
modded (use `ItemBase`, `CarScript`, ...), and `JsonSerializer` writes bool as
0/1, a null array as `[]` and a null object as `{}`; the sync body therefore
lists the parts it includes in `has`.
