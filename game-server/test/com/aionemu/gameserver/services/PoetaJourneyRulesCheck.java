package com.aionemu.gameserver.services;

import java.nio.file.*;
import java.util.*;
import javax.xml.bind.JAXBContext;
import com.aionemu.gameserver.dataholders.QuestsData;
import com.aionemu.gameserver.model.PlayerClass;
import com.aionemu.gameserver.model.templates.QuestTemplate;

/** Uses the actual quest XML, without loading a server or modifying a character. */
public final class PoetaJourneyRulesCheck {
	private static void check(boolean pass,String label) { if (!pass) throw new AssertionError(label); }
	public static void main(String[] args) throws Exception {
		QuestsData data = (QuestsData) JAXBContext.newInstance(QuestsData.class).createUnmarshaller().unmarshal(Path.of(args[0]).toFile());
		List<QuestTemplate> skipped = data.getQuestTemplates().stream().filter(PoetaJourneyRules::skippedQuest).toList();
		Set<Integer> expected = new TreeSet<>();
		for (int id=1000;id<=1006;id++) expected.add(id);
		for (int id=1100;id<=1129;id++) if (id != 1128) expected.add(id);
		expected.addAll(List.of(1205,1206,1207,1230,1231));
		Set<Integer> actual = new TreeSet<>(); for (QuestTemplate q : skipped) actual.add(q.getId());
		check(actual.equals(expected),"complete legitimate Poeta roster: " + actual);
		check(!actual.contains(1128) && !actual.contains(80158),"unused and event quests excluded");
		check(!actual.contains(1007),"playable Sanctum ceremony is excluded from skipped quests and rewards");
		Map<Integer,Long> mailed = new HashMap<>();
		for (QuestTemplate q : skipped) PoetaJourneyRules.rewardItems(q).forEach((id,count) -> mailed.merge(id,count,Long::sum));
		check(!mailed.containsKey(100000652) && !mailed.containsKey(100200614) && !mailed.containsKey(101700525),"ceremony weapon choices are not mailed early");
		check(PoetaJourneyRules.rewardItems(data.getQuestById(1007)).get(182400001) == 250000L,"ceremony currency only once across six identical groups");
		check(PoetaJourneyRules.rewardItems(data.getQuestById(1007)).get(162001057) == 5L,"ceremony potions only once");
		check(PoetaJourneyRules.rewardItems(data.getQuestById(1007)).get(100000652) == 1L,"shared selectable weapon once across classes");
		check(PoetaJourneyRules.rewardItems(data.getQuestById(1122)).keySet().containsAll(List.of(123000878,162000012,182000352)),"all branch reward items included");
		check(PoetaJourneyRules.rewardItems(data.getQuestById(1113)).keySet().containsAll(List.of(162000052,162000057,169000003,160003001)),"fixed and every selectable reward included");
		check(PoetaJourneyRules.rewardGold(data.getQuestById(1114)) == 1920L,"alternative gold awarded once at best completion value");
		int classes=0;
		for (PlayerClass base : PlayerClass.values()) if (base.isStartingClass()) {
			List<PlayerClass> choices = PoetaJourneyRules.advancedClasses(base);
			check(choices.size() == (base == PlayerClass.ARTIST ? 1 : 2),"class choice count for " + base);
			for (PlayerClass advanced : choices) { classes++;
				int dispatch = PoetaJourneyRules.dispatchQuest(advanced);
				check(data.getQuestById(dispatch) != null,"real dispatch quest for " + advanced);
				check(PoetaJourneyRules.ceremonyGroup(advanced) < data.getQuestById(1007).getRewards().size(),"valid ceremony group");
				check(data.getQuestById(1007).getSelectableRewardByClass(advanced).size() > 0,"class weapons available");
			}
		}
		check(classes == 11,"all eleven advanced classes supported");
		System.out.println("PASS: " + skipped.size() + " skipped quests; Sanctum ceremony rewards deferred; every reward branch and class choice; six onward dispatches.");
	}
}
