// SPDX-FileCopyrightText: 2026 Bernd Zeimetz <bernd@bzed.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

// What the event feed reports about bases: building, dismantling and destroying parts, flags and stashes.
// The hooks call super first and report afterwards, so a part that could not be built is not reported;
// only the stash hooks read the target before, because the action moves it.
class DZOAdminFeedHooks
{
	// A raid destroys the part and, right after, the parts that hang on it, all in one call without a
	// player. Those dependent parts are not events of their own.
	protected static Object s_RaidBase;
	protected static float s_RaidTime;

	protected static DZOAdminFeedEvent Build(string kind, Object at, Man player, string part)
	{
		DZOAdminFeedEvent e = DZOAdminFeed.New(kind);
		DZOAdminFeed.Actor(e, PlayerBase.Cast(player));
		DZOAdminFeed.Pos(e, at.GetPosition());
		e.object = part;
		e.text = at.GetType();
		return e;
	}

	static void Part(string kind, BaseBuildingBase base, Man player, string part)
	{
		if (!DZOAdminFeed.On() || !base)
			return;
		DZOAdminFeed.Add(Build(kind, base, player, part));
	}

	static void Destroyed(BaseBuildingBase base, Man player, string part, bool byConnectedPart)
	{
		if (!DZOAdminFeed.On() || !base || byConnectedPart)
			return;
		float now = GetGame().GetTickTime();
		if (player)
		{
			s_RaidBase = base;
			s_RaidTime = now;
		}
		else if (s_RaidBase == base && Math.AbsFloat(now - s_RaidTime) < 0.1)
		{
			return;
		}
		DZOAdminFeed.Add(Build("destroy", base, player, part));
	}

	static void LockCut(PlayerBase player, Fence fence, ItemBase tool)
	{
		DZOAdminFeedEvent e = DZOAdminFeed.New("lock_cut");
		DZOAdminFeed.Actor(e, player);
		DZOAdminFeed.Pos(e, fence.GetPosition());
		e.text = fence.GetType();
		if (tool)
			e.weapon = tool.GetDisplayName();
		DZOAdminFeed.Add(e);
	}

	static void Flag(string kind, TerritoryFlag flag, PlayerBase player)
	{
		if (!DZOAdminFeed.On() || !flag)
			return;
		DZOAdminFeedEvent e = DZOAdminFeed.New(kind);
		DZOAdminFeed.Actor(e, player);
		DZOAdminFeed.Pos(e, flag.GetPosition());
		e.object = flag.GetType();
		EntityAI cloth = flag.FindAttachmentBySlotName("Material_FPole_Flag");
		if (cloth)
			e.text = cloth.GetType();
		DZOAdminFeed.Add(e);
	}

	static void Stash(string kind, PlayerBase player, vector at, string item)
	{
		DZOAdminFeedEvent e = DZOAdminFeed.New(kind);
		DZOAdminFeed.Actor(e, player);
		DZOAdminFeed.Pos(e, at);
		e.object = item;
		DZOAdminFeed.Add(e);
	}
}

modded class BaseBuildingBase
{
	override void OnPartBuiltServer(notnull Man player, string part_name, int action_id)
	{
		super.OnPartBuiltServer(player, part_name, action_id);
		DZOAdminFeedHooks.Part("build", this, player, part_name);
	}

	override void OnPartDismantledServer(notnull Man player, string part_name, int action_id)
	{
		super.OnPartDismantledServer(player, part_name, action_id);
		DZOAdminFeedHooks.Part("dismantle", this, player, part_name);
	}

	override void OnPartDestroyedServer(Man player, string part_name, int action_id, bool destroyed_by_connected_part = false)
	{
		super.OnPartDestroyedServer(player, part_name, action_id, destroyed_by_connected_part);
		DZOAdminFeedHooks.Destroyed(this, player, part_name, destroyed_by_connected_part);
	}
}

// The flag's own raising and lowering by a player (vanilla calls this only with a player; dzo's flag_raise
// has none and is in the audit log).
modded class TerritoryFlag
{
	// The mast's phase is inverted: 0 is at the top.
	override protected void LogAnimateFlag(float newPhase, notnull PlayerBase player)
	{
		super.LogAnimateFlag(newPhase, player);
		if (newPhase == 0)
			DZOAdminFeedHooks.Flag("flag_raised", this, player);
		else if (newPhase == 1)
			DZOAdminFeedHooks.Flag("flag_lowered", this, player);
	}
}

// Cutting the combination lock of a fence with a saw. The class is deprecated in 1.30 and no item offers
// it; where a mod brings the action back it is reported.
modded class ActionDestroyCombinationLock
{
	override void OnFinishProgressServer(ActionData action_data)
	{
		Fence fence = Fence.Cast(action_data.m_Target.GetObject());
		CombinationLock combo;
		if (fence)
			combo = fence.GetCombinationLock();
		super.OnFinishProgressServer(action_data);
		if (DZOAdminFeed.On() && fence && combo && combo.IsDamageDestroyed())
			DZOAdminFeedHooks.LockCut(action_data.m_Player, fence, action_data.m_MainItem);
	}
}

modded class ActionDigInStash
{
	override void OnFinishProgressServer(ActionData action_data)
	{
		EntityAI item = EntityAI.Cast(action_data.m_Target.GetObject());
		string type;
		vector at;
		if (item)
		{
			type = item.GetType();
			at = item.GetPosition();
		}
		super.OnFinishProgressServer(action_data);
		// The item is in the stash now; a failed dig leaves it where it was.
		if (DZOAdminFeed.On() && item && UndergroundStash.Cast(item.GetHierarchyParent()))
			DZOAdminFeedHooks.Stash("stash_buried", action_data.m_Player, at, type);
	}
}

modded class ActionDigOutStash
{
	override void OnFinishProgressServer(ActionData action_data)
	{
		UndergroundStash stash = UndergroundStash.Cast(action_data.m_Target.GetObject());
		string type;
		vector at;
		if (stash)
		{
			at = stash.GetPosition();
			ItemBase stashed = stash.GetStashedItem();
			if (stashed)
				type = stashed.GetType();
		}
		super.OnFinishProgressServer(action_data);
		if (DZOAdminFeed.On() && type != "")
			DZOAdminFeedHooks.Stash("stash_dug_up", action_data.m_Player, at, type);
	}
}
