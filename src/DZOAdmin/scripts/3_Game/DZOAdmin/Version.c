// SPDX-FileCopyrightText: 2026 Bernd Zeimetz <bernd@bzed.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

// Protocol version of the dzo <-> dzo-admin `hello` handshake.
const int DZOADMIN_PROTOCOL_VERSION = 1;

class DZOAdminVersion
{
	static void LogLoaded()
	{
		Print("[DZOAdmin] loaded, protocol " + DZOADMIN_PROTOCOL_VERSION);
	}
}
