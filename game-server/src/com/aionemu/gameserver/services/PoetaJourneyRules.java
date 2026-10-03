package com.aionemu.gameserver.services;

import java.util.*;
import com.aionemu.gameserver.model.PlayerClass;
import com.aionemu.gameserver.model.Race;
import com.aionemu.gameserver.model.templates.QuestTemplate;
import com.aionemu.gameserver.model.templates.quest.QuestItems;

/** Builds a single-completion reward bundle, including every alternative item. */
public final class PoetaJourneyRules {
	public static final int KINAH = 182400001;
	private PoetaJourneyRules() {}

	public static boolean skippedQuest(QuestTemplate q) {
		return q.getRacePermitted() == Race.ELYOS && !q.isRestricted() && !q.isNoCount()
			&& q.getMinlevelPermitted() <= 9 && q.getMaxRepeatCount() == 1
			&& q.getId() != 1007
			&& ("Poeta".equals(q.getQuestZone()) || q.getId() == 1006);
	}

	public static List<PlayerClass> advancedClasses(PlayerClass base) {
		return Arrays.stream(PlayerClass.values()).filter(c -> !c.isStartingClass() && c.getStartingClass() == base).toList();
	}

	public static int ceremonyGroup(PlayerClass advanced) {
		return switch (advanced.getStartingClass()) {
			case WARRIOR -> 0; case SCOUT -> 1; case MAGE -> 2;
			case PRIEST -> 3; case ENGINEER -> 4; case ARTIST -> 5;
			default -> throw new IllegalArgumentException("Choose an advanced class.");
		};
	}

	public static int dispatchQuest(PlayerClass advanced) {
		return new int[] {1913, 1914, 1915, 1916, 19070, 19071}[ceremonyGroup(advanced)];
	}

	public static Map<Integer,Long> rewardItems(QuestTemplate q) {
		Map<Integer,Long> result = new LinkedHashMap<>();
		for (var reward : q.getRewards()) {
			Map<Integer,Long> group = new LinkedHashMap<>();
			for (QuestItems item : reward.getRewardItem()) group.merge(item.getItemId(), item.getCount(), Long::sum);
			for (QuestItems item : reward.getSelectableRewardItem()) group.merge(item.getItemId(), item.getCount(), Math::max);
			group.forEach((id,count) -> result.merge(id,count,Math::max));
		}
		// All selectable weapons, including the other class choices, are requested.
		// Shared alternatives are awarded once per quest, rather than once per class.
		for (PlayerClass c : PlayerClass.values())
			for (QuestItems item : q.getSelectableRewardByClass(c)) result.merge(item.getItemId(), item.getCount(), Math::max);
		return result;
	}

	public static long rewardGold(QuestTemplate q) {
		return q.getRewards().stream().mapToLong(r -> r.getKinah()).max().orElse(0);
	}
}
