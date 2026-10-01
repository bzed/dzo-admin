// SPDX-FileCopyrightText: 2026 Bernd Zeimetz <bernd@bzed.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

// Protocol version of the dzo <-> dzo-admin `hello` handshake. Bump it on
// every incompatible change of the JSON in Proto.c.
const int DZOADMIN_PROTOCOL_VERSION = 1;
const string DZOADMIN_VERSION = "0.1.0";
const string DZOADMIN_CONFIG_PATH = "$profile:dzo-admin/config.json";

class DZOAdminVersion
{
	static void LogLoaded()
	{
		Print("[DZOAdmin] loaded, version " + DZOADMIN_VERSION + ", protocol " + DZOADMIN_PROTOCOL_VERSION);
	}
}
