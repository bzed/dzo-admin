<!--
SPDX-FileCopyrightText: 2026 Bernd Zeimetz <bernd@bzed.de>
SPDX-License-Identifier: AGPL-3.0-or-later
-->

# marker-demo

A tiny example mod that puts markers on the dzo admin map **without a hard
dependency** on dzo-admin. Copy `MarkerDemo/` and adapt it.

The trick is the `DZO_ADMIN` define: dzo-admin sets it in `CfgMods`
(`defines[]`), so code inside `#ifdef DZO_ADMIN` is only compiled when
dzo-admin is loaded. Without it the mod still works, it just draws nothing.
Do not list `DZOAdmin` in `requiredAddons`.

If your mod cannot use `#ifdef` (it loads before dzo-admin), write
`$profile:dzo-admin/markers/<layer>.json` instead; dzo reads those files.
