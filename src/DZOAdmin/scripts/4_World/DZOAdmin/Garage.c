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

	// Brings a stored vehicle back into the world from its entity-storage file, at the place its
	// garage entry keeps. deleteRestored = false is the safety net: the file survives a failed
	// action, so the vehicle is not lost when what follows goes wrong. The placeholder (the
	// invisible marker left where the vehicle was stored) is only deleted on request: its delete
	// removes the entity-storage file in the next frame (EEDelete), which Retrieve wants (it is
	// done with the file) but Move must not (it writes the vehicle back to that very file).
	protected static bool Restore(ExpansionGarageVehicleData vehicleData, out EntityAI entity, bool deletePlaceholder)
	{
		ExpansionEntityStoragePlaceholder placeholder = ExpansionEntityStoragePlaceholder.GetByStoredEntityGlobalID(vehicleData.m_GlobalID);
		if (!ExpansionEntityStorageModule.RestoreFromFile(vehicleData.GetEntityStorageFileName(), entity, placeholder, null, false))
			return false;
		if (!entity)
			return false; // a corrupt storage file can restore without creating the entity
		// The placeholder is invisible and damage-immune; the vehicle carries its cargo again.
		// (There is none when the garage stores the cargo virtually.)
		if (deletePlaceholder && placeholder)
			placeholder.Delete();
		return true;
	}

	// Tells the online owners of a stored vehicle what happened to it, the way the mod's other
	// Expansion notifications are sent (DZOAdminWorld.SendExpansion).
	protected static void NotifyOwners(ExpansionGarageVehicleData vehicleData, string text)
	{
		if (!vehicleData.m_OwnerUIDs)
			return;
		string displayName = ExpansionStatic.GetItemDisplayNameWithType(vehicleData.m_ClassName);
		foreach (string uid : vehicleData.m_OwnerUIDs)
		{
			PlayerBase p = PlayerBase.GetPlayerByUID(uid);
			if (p && p.GetIdentity())
				ExpansionNotification("Server", "Your " + displayName + " " + text, "", ARGB(255, 52, 152, 219), 7, ExpansionNotificationType.GARAGE).Create(p.GetIdentity());
		}
	}

	// Takes a stored vehicle out of its garage and puts it on the ground at x/z.
	static string Retrieve(DZOAdminCommand c)
	{
		ExpansionGarageData garageData;
		ExpansionGarageVehicleData vehicleData;
		if (!FindStored(c.vehicle, garageData, vehicleData))
			return "stored vehicle not found";
		float size = GetGame().GetWorld().GetWorldSize();
		if (c.x < 0 || c.z < 0 || c.x > size || c.z > size)
			return "position is outside the world";
		EntityAI entity;
		if (!Restore(vehicleData, entity, true))
			return "could not restore it from its storage file";
		DZOAdminWorld.PlaceVehicle(entity, c.x, c.z);
		// Best effort, the return is not checked: the restore left the file (Restore keeps it until
		// here so a failed action cannot lose the vehicle), and Expansion's own LoadVehicle deletes
		// it the same way once the vehicle is out. When it stays, the vehicle is in the world anyway.
		DeleteFile(vehicleData.GetEntityStorageFileName());
		garageData.RemoveVehicle(vehicleData);
		garageData.Save();
		NotifyOwners(vehicleData, "was taken out of the garage by an admin");
		return "";
	}

	// Moves the spawn point of a stored vehicle: where it stands when its owner takes it out.
	static string Move(DZOAdminCommand c)
	{
		ExpansionGarageData garageData;
		ExpansionGarageVehicleData vehicleData;
		if (!FindStored(c.vehicle, garageData, vehicleData))
			return "stored vehicle not found";
		float size = GetGame().GetWorld().GetWorldSize();
		if (c.x < 0 || c.z < 0 || c.x > size || c.z > size)
			return "position is outside the world";
		Object blocking;
		if (!ExpansionItemSpawnHelper.IsSpawnPositionFree(Vector(c.x, DZOAdminWorld.TopY(c.x, c.z), c.z), vehicleData.m_Orientation, vehicleData.m_ClassName, blocking))
		{
			string displayName;
			PlayerBase blockingPlayer = PlayerBase.Cast(blocking);
			if (blockingPlayer && blockingPlayer.GetIdentity())
				displayName = blockingPlayer.GetIdentityName(); // so the owner knows who stands there
			else if (blocking)
				displayName = blocking.GetDisplayName();
			return "the spawn point is blocked by " + displayName;
		}
		// The placeholder is kept alive for now: deleting it would remove the entity-storage file
		// in the next frame (EEDelete) - the very file the vehicle is written back to below.
		ExpansionEntityStoragePlaceholder placeholder = ExpansionEntityStoragePlaceholder.GetByStoredEntityGlobalID(vehicleData.m_GlobalID);
		EntityAI entity;
		if (!Restore(vehicleData, entity, false))
			return "could not restore it from its storage file";
		DZOAdminWorld.PlaceVehicle(entity, c.x, c.z); // keeps the stored orientation
		if (!ExpansionEntityStorageModule.SaveToFile(entity, vehicleData.GetEntityStorageFileName()))
		{
			// The restore moved the cargo from the placeholder onto the vehicle; put it back the way
			// Expansion's own restore failure does, so it does not die with the vehicle. If that
			// fails too, the cargo is lost, but the file still holds the old (working) state.
			if (placeholder && entity.HasAnyCargo() && !MiscGameplayFunctions.Expansion_MoveCargo(entity, placeholder))
				EXError.Error(null, "Couldn't move cargo from " + entity + " back to placeholder " + placeholder);
			GetGame().ObjectDelete(entity);
			return "could not save the vehicle back; its spawn point is unchanged";
		}
		// The garage entry gets the new place, the way Expansion's SetDataFromVehicleObject does:
		// the stored y is the model zero point (the vehicle's lowest point), not where the physics sits.
		float modelZeroPoint = 0;
		ExpansionVehicle ev;
		if (ExpansionVehicle.Get(ev, entity))
			modelZeroPoint = ev.GetModelZeroPointDistanceFromGround();
		vector p = entity.GetPosition();
		vehicleData.m_Position = Vector(p[0], p[1] - modelZeroPoint, p[2]);
		vehicleData.m_Orientation = entity.GetOrientation();
		garageData.Save();
		// The placeholder follows the spawn point, then the vehicle object goes away again; the
		// placeholder stays and marks the new place for the next retrieve.
		if (placeholder)
			placeholder.SetPosition(entity.GetPosition());
		GetGame().ObjectDelete(entity);
		NotifyOwners(vehicleData, "had its garage spawn point moved by an admin");
		return "";
	}
}
#endif
