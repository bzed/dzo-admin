// SPDX-FileCopyrightText: 2026 Bernd Zeimetz <bernd@bzed.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

// What heal needs from the player: the state the game keeps in protected members.
modded class PlayerBase
{
	// Heals what the game's own Cupid's bolt heals (PlayerBase.EEHitBy): every damage zone, the modifiers
	// (diseases, broken bones), bleeding, the stats except water, energy and temperature, the agents, the
	// stamina and unconsciousness. Food and water stay as they are: healing is not feeding.
	void DZOAdminHeal()
	{
		DamageSystem.ResetAllZones(this);
		GetModifiersManager().ResetAll();
		if (m_BleedingManagerServer)
			m_BleedingManagerServer.RemoveAllSources();
		if (GetPlayerStats())
		{
			int bloodType = GetStatBloodType().Get();
			float energyValue = GetStatEnergy().Get();
			float waterValue = GetStatWater().Get();
			float heatBuffer = GetStatHeatBuffer().Get();
			float heatComfort = GetStatHeatComfort().Get();
			float thermalBias = GetThermalBiasHandler().Get();
			GetPlayerStats().ResetAllStats();
			GetStatBloodType().Set(bloodType);
			GetStatWater().Set(waterValue);
			GetStatEnergy().Set(energyValue);
			GetStatHeatBuffer().Set(heatBuffer);
			GetStatHeatComfort().Set(heatComfort);
			GetThermalBiasHandler().Set(thermalBias);
		}
		if (m_AgentPool)
			m_AgentPool.RemoveAllAgents();
		if (m_StaminaHandler)
			m_StaminaHandler.SetStamina(GameConstants.STAMINA_MAX);
		if (IsUnconscious())
			DayZPlayerSyncJunctures.SendPlayerUnconsciousness(this, false);
	}
}
