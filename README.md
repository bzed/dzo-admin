<!--
SPDX-FileCopyrightText: 2026 Bernd Zeimetz <bernd@bzed.de>
SPDX-License-Identifier: AGPL-3.0-or-later
-->

# dzo-admin

Server-side DayZ admin mod for [dayz-server-operator](https://github.com/bzed-ai/dayz-server-operator)
(`dzo`). It is a **servermod** (never sent to clients, no signing needed), and
is built into a PBO by the `dzo` repository, which carries this repository as
a submodule at `servermods/dzo-admin`.

Status: skeleton. It only prints a "loaded" line on the server script log.

Layout: `src/DZOAdmin/` is the PBO root (`$PBOPREFIX$` = `DZOAdmin`).
