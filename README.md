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
| | `Config.c` | `$profile:dzo-admin/config.json` |
| | `Transport.c` | outbound `RestApi` POST with backoff |
| | `Map.c` | `DZOAdmin_Map`, the marker API for other mods |
| `4_World` | `Hooks.c` | vehicle/effect-area registries, class watch rules |
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
`teleport`, `spawn_item`, `vehicle_repair`, `vehicle_delete`; the field list is
`DZOAdminCommand` in `Proto.c`. A command id the mod has already executed is
answered from memory, not executed again.

## Map markers for other mods

See `examples/marker-demo`. Three ways: class watch rules in the config (no
code), the `DZOAdmin_Map` API behind `#ifdef DZO_ADMIN`, or JSON files in
`$profile:dzo-admin/markers/` (read by dzo directly).

## Status

Tested on DayZ 1.29 (stable) and 1.30 (experimental) against the real dzo
endpoint, with a real DayZ client (`scripts/test-client.sh` in the dzo
repository): it compiles, loads and syncs; the players state, messages in all
three styles, teleport, `spawn_item` (inventory, hands, ground), the refusals,
class watch rules on items, the crew of a vehicle, and vehicle repair/delete
work. See the dzo README, "Status".

Enforce Script reminders from the boot: `proto` and `out` are reserved words
(even as field or parameter names), `EntityAI` is an engine class and cannot be
modded (use `ItemBase`, `CarScript`, ...), and `JsonSerializer` writes bool as
0/1, a null array as `[]` and a null object as `{}`; the sync body therefore
lists the parts it includes in `has`.
