// SPDX-FileCopyrightText: 2026 Bernd Zeimetz <bernd@bzed.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

// The vehicles stored in the DayZ Expansion virtual garage. They are not in the world (only their
// entity-storage file and their garage data), so they are read from the garage module for the map
// and the storedvehicle actions. All accessors are null-checked.
#ifdef EXPANSIONMODGARAGE
class DZOAdminGarage
{
	// The "g" keeps the ids from looking like those of the vehicles in the world (DZOAdminWorld.VehicleId).
	static string StoredId(ExpansionGarageVehicleData vehicleData)
	{
		return "g" + vehicleData.m_GlobalID[0].ToString() + "." + vehicleData.m_GlobalID[1].ToString() + "." + vehicleData.m_GlobalID[2].ToString() + "." + vehicleData.m_GlobalID[3].ToString();
	}

	// The garage and vehicle data of a stored id, or false when there is none (the garage module not
	// loaded, or no garage holds a vehicle with that id).
	static bool FindStored(string id, out ExpansionGarageData garageData, out ExpansionGarageVehicleData vehicleData)
	{
		garageData = null;
		vehicleData = null;
		if (!ExpansionGarageModule.s_Instance)
			return false;
		array<ref ExpansionGarageData> garages = ExpansionGarageModule.s_Instance.GetGarageData();
		if (!garages)
			return false;
		foreach (ExpansionGarageData garage : garages)
		{
			if (!garage)
				continue;
			foreach (ExpansionGarageVehicleData vd : garage.m_Vehicles)
			{
				if (!vd || !vd.IsGlobalIDValid())
					continue;
				if (StoredId(vd) == id)
				{
					garageData = garage;
					vehicleData = vd;
					return true;
				}
			}
		}
		return false;
	}

	// Every vehicle of every garage, for the vehicles part of the sync. A vehicle whose entity-storage
	// file is gone can never be retrieved again (Expansion refuses it); it is reported as broken.
	static void Collect(array<ref DZOAdminVehicle> list)
	{
		if (!ExpansionGarageModule.s_Instance)
			return;
		array<ref ExpansionGarageData> garages = ExpansionGarageModule.s_Instance.GetGarageData();
		if (!garages)
			return;
		foreach (ExpansionGarageData garage : garages)
		{
			if (!garage)
				continue;
			foreach (ExpansionGarageVehicleData vd : garage.m_Vehicles)
			{
				if (!vd || !vd.IsGlobalIDValid())
					continue;
				DZOAdminVehicle v = new DZOAdminVehicle();
				v.id = StoredId(vd);
				v.type = vd.m_ClassName; // kind stays empty: dzo classifies by class name
				v.x = vd.m_Position[0];
				v.y = vd.m_Position[1];
				v.z = vd.m_Position[2];
				v.yaw = vd.m_Orientation[0];
				v.stored = true;
				v.stored_by = garage.m_OwnerUID;
				v.territory_id = garage.m_TerritoryID;
				if (v.territory_id < 0)
					v.territory_id = 0; // no territory
				if (vd.m_OwnerUIDs)
				{
					v.owners = new array<string>;
					foreach (string uid : vd.m_OwnerUIDs)
						v.owners.Insert(uid);
				}
				v.broken = !FileExist(vd.GetEntityStorageFileName());
				list.Insert(v);
			}
		}
	}
}
#endif
