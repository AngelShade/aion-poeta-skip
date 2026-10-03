package com.aionemu.gameserver.services;

import java.nio.file.*;
import java.sql.*;
import java.util.*;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import com.aionemu.commons.database.DatabaseFactory;
import com.aionemu.gameserver.configs.main.CustomConfig;
import com.aionemu.gameserver.dao.*;
import com.aionemu.gameserver.dataholders.DataManager;
import com.aionemu.gameserver.model.PlayerClass;
import com.aionemu.gameserver.model.Race;
import com.aionemu.gameserver.model.gameobjects.Item;
import com.aionemu.gameserver.model.gameobjects.Persistable.PersistentState;
import com.aionemu.gameserver.model.gameobjects.player.Player;
import com.aionemu.gameserver.model.gameobjects.player.Rates;
import com.aionemu.gameserver.model.gameobjects.state.CreatureState;
import com.aionemu.gameserver.model.items.storage.StorageType;
import com.aionemu.gameserver.model.templates.QuestTemplate;
import com.aionemu.gameserver.network.aion.serverpackets.*;
import com.aionemu.gameserver.network.aion.serverpackets.SM_QUEST_ACTION.ActionType;
import com.aionemu.gameserver.questEngine.QuestEngine;
import com.aionemu.gameserver.questEngine.model.*;
import com.aionemu.gameserver.services.item.ItemFactory;
import com.aionemu.gameserver.services.mail.MailService;
import com.aionemu.gameserver.services.player.PlayerService;
import com.aionemu.gameserver.services.teleport.TeleportService;
import com.aionemu.gameserver.utils.PacketSendUtility;
import com.aionemu.gameserver.utils.idfactory.IDFactory;
import com.aionemu.gameserver.world.World;

/** Optional starter journey. Rewards, completion and the decision commit together. */
public final class PoetaJourneyService {
	private static final Logger log = LoggerFactory.getLogger(PoetaJourneyService.class);
	private static volatile boolean ready;
	static record MailAward(int mailId,Object[] item,long kinah) {}
	private PoetaJourneyService() {}

	public static void start() throws Exception {
		if (!CustomConfig.ENABLE_POETA_JOURNEY) return;
		try (Connection c = DatabaseFactory.getConnection(); Statement s = c.createStatement()) {
			for (String sql : Files.readString(Path.of("config/journey/schema.sql")).split(";")) if (!sql.isBlank()) s.execute(sql);
			migrate(c);
			try (ResultSet r = s.executeQuery("SELECT TABLE_NAME,ENGINE FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME IN ('players','player_quests','player_titles','inventory','mail','player_bind_point','poeta_journey')")) {
				int count = 0;
				while (r.next()) { count++; if (!"InnoDB".equalsIgnoreCase(r.getString(2))) throw new SQLException("Journey requires InnoDB: " + r.getString(1)); }
				if (count != 7) throw new SQLException("Journey persistence tables are missing.");
			}
		}
		for (QuestTemplate q : quests()) for (int id : PoetaJourneyRules.rewardItems(q).keySet())
			if (DataManager.ITEM_DATA.getItemTemplate(id) == null) throw new IllegalStateException("Missing journey reward " + id);
		ready = true;
		log.info("Poeta journey ready: {} quests, optional level-10 skip and mailed rewards", quests().size());
	}

	public static List<QuestTemplate> quests() {
		return DataManager.QUEST_DATA.getQuestTemplates().stream().filter(PoetaJourneyRules::skippedQuest).sorted(Comparator.comparingInt(QuestTemplate::getId)).toList();
	}

	public static boolean eligible(Player p) {
		return ready && p.getRace() == Race.ELYOS && p.getPlayerClass().isStartingClass() && p.getLevel() <= 9
			&& p.getWorldId() == 210010000 && !p.isDead() && p.getCommonData().getLastTransferTime() == 0;
	}

	public static boolean awaitingChoice(Player p) {
		if (!eligible(p)) return false;
		try (Connection c = DatabaseFactory.getConnection()) { return decision(c,p.getObjectId()).isEmpty(); }
		catch (SQLException e) { log.error("Could not read journey choice",e); return false; }
	}

	private static String decision(Connection c,int id) throws SQLException {
		try (PreparedStatement s = c.prepareStatement("SELECT decision FROM poeta_journey WHERE player_id=?")) {
			s.setInt(1,id); try (ResultSet r = s.executeQuery()) { return r.next() ? r.getString(1) : ""; }
		}
	}

	static Map<String,Object> snapshot(Player p) throws SQLException {
		try (Connection c = DatabaseFactory.getConnection()) {
			String choice = decision(c,p.getObjectId());
			List<Map<String,Object>> classes = PoetaJourneyRules.advancedClasses(p.getPlayerClass()).stream()
				.map(v -> Map.<String,Object>of("id",v.name(),"name",className(v))).toList();
			int pending = (int) quests().stream().filter(q -> q.getId() != 1007 && !completed(p,q.getId())).count();
			return new LinkedHashMap<>(Map.of("eligible",eligible(p),"prompt",eligible(p) && choice.isEmpty(),"decision",choice,
				"name",p.getName(),"classes",classes,"quests",pending,"level",p.getLevel(),"welcome",welcomePending(c,p.getObjectId()),
				"ceremonyRewardsMailed",ceremonyRewardsMailed(p)));
		}
	}

	private static String className(PlayerClass c) {
		return switch (c) { case RIDER -> "Aethertech"; case GUNNER -> "Gunslinger"; case BARD -> "Songweaver";
			case SPIRIT_MASTER -> "Spiritmaster"; default -> c.name().substring(0,1) + c.name().substring(1).toLowerCase(Locale.ROOT); };
	}

	private static boolean completed(Player p,int id) {
		QuestState q = p.getQuestStateList().getQuestState(id); return q != null && q.getCompleteCount() > 0;
	}

	static String choose(Player p,String action,String selected) throws Exception {
		synchronized (p) {
			if (action.equals("ack")) {
				if (!p.isOnline() || World.getInstance().getPlayer(p.getObjectId()) != p) throw new IllegalArgumentException("Log in to continue.");
				try (Connection c = DatabaseFactory.getConnection()) { acknowledge(c,p.getObjectId()); }
				return "Enjoy your journey!";
			}
			if (!p.isOnline() || World.getInstance().getPlayer(p.getObjectId()) != p || !eligible(p) || !p.isSpawned()
				|| !CreatureState.isStanding(p.getState()) || p.getController().isInCombat())
				throw new IllegalArgumentException("Choose while standing safely in Poeta, before Ascension.");
			if (action.equals("play")) {
				try (Connection c = DatabaseFactory.getConnection()) {
					if (decision(c,p.getObjectId()).equals("SKIP")) throw new IllegalArgumentException("Your journey has already begun.");
					update(c,"INSERT INTO poeta_journey(player_id,decision) VALUES(?,'PLAY') ON DUPLICATE KEY UPDATE decision='PLAY'",p.getObjectId());
				}
				QuestEngine.getInstance().onEnterWorld(p);
				return "Your story begins in Poeta. Enjoy the journey!";
			}
			if (!action.equals("skip")) throw new IllegalArgumentException("Choose a journey.");
			PlayerClass advanced;
			try { advanced = PlayerClass.valueOf(selected); } catch (Exception e) { throw new IllegalArgumentException("Choose your advanced class."); }
			if (!PoetaJourneyRules.advancedClasses(p.getPlayerClass()).contains(advanced)) throw new IllegalArgumentException("This class does not belong to your character.");
			return skip(p,advanced);
		}
	}

	private static String skip(Player p,PlayerClass advanced) throws Exception {
		List<QuestTemplate> pending = quests().stream().filter(q -> !completed(p,q.getId())).toList();
		Map<Integer,Long> rewards = new LinkedHashMap<>();
		Set<Integer> titles = new LinkedHashSet<>();
		int expansion = 0;
		for (QuestTemplate q : pending) {
			PoetaJourneyRules.rewardItems(q).forEach((id,count) -> rewards.merge(id,count,Long::sum));
			long gold = Rates.QUEST_KINAH.calcResult(p,PoetaJourneyRules.rewardGold(q));
			if (gold > 0) rewards.merge(PoetaJourneyRules.KINAH,gold,Long::sum);
			for (var r : q.getRewards()) if (r.getTitle() != 0) titles.add(r.getTitle());
		}
		// Each completed quest expands at most once, even if it has alternative groups.
		expansion = (int) pending.stream().filter(q -> q.getRewards().stream().anyMatch(r -> r.getExtendInventory() == 1)).count();
		List<Item> attachments = new ArrayList<>();
		List<Integer> allocated = new ArrayList<>();
		long kinah = rewards.getOrDefault(PoetaJourneyRules.KINAH,0L);
		rewards.remove(PoetaJourneyRules.KINAH);
		boolean committed = false;
		try {
			for (var reward : rewards.entrySet()) {
				var template = DataManager.ITEM_DATA.getItemTemplate(reward.getKey());
				if (template == null || reward.getValue() <= 0) throw new IllegalStateException("Invalid reward " + reward.getKey());
				for (long count = reward.getValue(); count > 0;) {
					long stack = Math.min(count,Math.max(1,template.getMaxStackCount()));
					Item item = ItemFactory.newItem(reward.getKey(),stack);
					if (item == null) throw new IllegalStateException("Could not create journey reward.");
					item.setItemLocation(StorageType.MAILBOX.getId()); attachments.add(item); allocated.add(item.getObjectId()); count -= stack;
				}
			}
			// Flush previously earned inventory before writing reward attachments.
			if (!InventoryDAO.store(p)) throw new SQLException("Could not save inventory before the journey.");
			List<MailAward> mails = new ArrayList<>();
			for (Item item : attachments) {
				int id = IDFactory.getInstance().nextId(); allocated.add(id);
				mails.add(new MailAward(id,itemFields(p.getObjectId(),item),0));
			}
			if (kinah > 0 || attachments.isEmpty()) {
				int id = IDFactory.getInstance().nextId(); allocated.add(id); mails.add(new MailAward(id,null,kinah));
			}
			try (Connection c = DatabaseFactory.getConnection()) {
				commitSkip(c,p.getObjectId(),advanced,DataManager.PLAYER_EXPERIENCE_TABLE.getStartExpForLevel(10),p.getQuestExpands()+expansion,pending,titles,mails);
				committed = true;
			}
			// The ceremony is played in Sanctum; start it without mailing its rewards.
			List<QuestTemplate> progression = new ArrayList<>(pending);
			if (!completed(p,1007)) progression.add(DataManager.QUEST_DATA.getQuestById(1007));
			for (QuestTemplate q : progression) {
				QuestState old = p.getQuestStateList().getQuestState(q.getId());
				QuestState state = old == null ? new QuestState(q.getId(),QuestStatus.COMPLETE) : old;
				state.setStatus(q.getId() == 1007 ? QuestStatus.START : QuestStatus.COMPLETE); state.setQuestVar(q.getId() == 1007 ? 1 : 0); state.setFlags(0);
				if (q.getId() == 1007) state.setCompleteCount(0);
				state.setRewardGroup(q.getId() == 1007 ? PoetaJourneyRules.ceremonyGroup(advanced) : 0);
				state.setPersistentState(PersistentState.UPDATED);
				if (old == null) p.getQuestStateList().addQuest(q.getId(),state);
				if (old != null || q.getId() == 1007) PacketSendUtility.sendPacket(p,new SM_QUEST_ACTION(old == null ? ActionType.ADD : ActionType.UPDATE,state));
			}
			ClassChangeService.setClass(p,advanced,true,true);
			p.getCommonData().setLevel(10);
			p.getCommonData().setQuestExpands(p.getQuestExpands()+expansion); p.setCubeLimit();
			PacketSendUtility.sendPacket(p,SM_CUBE_UPDATE.cubeSize(StorageType.CUBE,p));
			for (int title : titles) if (!p.getTitleList().contains(title)) p.getTitleList().addEntry(title,0);
			PacketSendUtility.sendPacket(p,new SM_TITLE_INFO(p));
			int dispatch = PoetaJourneyRules.dispatchQuest(advanced);
			QuestService.addOrUpdateQuest(p,dispatch,QuestStatus.START);
			p.getQuestStateList().getQuestState(dispatch).setPersistentState(PersistentState.UPDATED);
			QuestEngine.getInstance().sendCompletedQuests(p);
			PacketSendUtility.sendPacket(p,new SM_QUEST_LIST(p.getQuestStateList().getUncompletedQuests()));
			p.getController().updateNearbyQuests();
			PlayerBindPointDAO.loadBindPoint(p);
			MailService.onPlayerLogin(p);
			p.getCommonData().setMailboxLetters(p.getMailbox().size());
			TeleportService.teleportTo(p,110010000,1313f,1512f,568f,(byte)0);
			PlayerService.storePlayer(p);
			log.info("Poeta journey: {} chose {}; {} quests completed and {} reward stacks mailed",p.getName(),advanced,pending.size(),attachments.size());
			return "Welcome to Sanctum! Collect your skipped Poeta quest rewards from the mailbox. Speak to Leah for A Ceremony in Sanctum and earn its rewards when you complete it, then see Polyidus for Dispatch to Verteron.";
		} catch (Exception e) {
			if (!committed) for (int id : allocated) IDFactory.getInstance().releaseId(id);
			else {
				log.error("Journey committed; closing {} to reload saved progression",p,e);
				try (Connection c = DatabaseFactory.getConnection()) { update(c,"UPDATE poeta_journey SET needs_recovery=1 WHERE player_id=?",p.getObjectId()); }
				catch (SQLException failure) { log.error("Could not mark journey recovery",failure); }
				p.getClientConnection().close();
			}
			throw e;
		}
	}

	static void saveQuest(Connection c,int player,int quest,String status,int group) throws SQLException {
		update(c,"INSERT INTO player_quests(player_id,quest_id,status,quest_vars,flags,complete_count,reward,complete_time) VALUES(?,?,?,0,0,?,?,?) ON DUPLICATE KEY UPDATE status=VALUES(status),quest_vars=0,flags=0,complete_count=GREATEST(complete_count,VALUES(complete_count)),reward=VALUES(reward),complete_time=COALESCE(complete_time,VALUES(complete_time))",
			player,quest,status,status.equals("COMPLETE") ? 1 : 0,group,status.equals("COMPLETE") ? new Timestamp(System.currentTimeMillis()) : null);
	}

	static void commitSkip(Connection c,int player,PlayerClass advanced,long exp,int expansions,List<QuestTemplate> quests,Set<Integer> titles,List<MailAward> mails) throws SQLException {
		c.setAutoCommit(false);
		try {
			try (PreparedStatement s = c.prepareStatement("SELECT id FROM players WHERE id=? FOR UPDATE")) {
				s.setInt(1,player); try (ResultSet r = s.executeQuery()) { if (!r.next()) throw new IllegalArgumentException("Character is unavailable."); }
			}
			if (decision(c,player).equals("SKIP")) throw new IllegalArgumentException("This skip has already been awarded.");
			int oldMailCount;
			try (PreparedStatement s = c.prepareStatement("SELECT COUNT(*) FROM mail WHERE mail_recipient_id=?")) {
				s.setInt(1,player); try (ResultSet r = s.executeQuery()) { r.next(); oldMailCount=r.getInt(1); }
			}
			if (oldMailCount + mails.size() > 200) throw new IllegalArgumentException("Collect existing mail first. The full reward bundle needs " + mails.size() + " free letters.");
			for (QuestTemplate q : quests) {
				saveQuest(c,player,q.getId(),"COMPLETE",0);
			}
			saveCeremony(c,player,advanced);
			saveQuest(c,player,PoetaJourneyRules.dispatchQuest(advanced),"START",0);
			for (MailAward award : mails) {
				int item = 0;
				if (award.item() != null) {
					try (PreparedStatement s = c.prepareStatement(InventoryDAO.INSERT_QUERY)) {
						for (int i=0;i<award.item().length;i++) s.setObject(i+1,award.item()[i]); s.executeUpdate();
					}
					item = ((Number)award.item()[0]).intValue();
				}
				mail(c,player,award.mailId(),item,award.kinah());
			}
			for (int title : titles) update(c,"INSERT IGNORE INTO player_titles(player_id,title_id,remaining) VALUES(?,?,0)",player,title);
			update(c,"UPDATE players SET player_class=?,exp=?,old_level=10,world_id=110010000,world_owner=0,x=1313,y=1512,z=568,heading=0,quest_expands=?,mailbox_letters=? WHERE id=?",
				advanced.name(),exp,expansions,oldMailCount+mails.size(),player);
			update(c,"REPLACE INTO player_bind_point(player_id,map_id,x,y,z,heading) VALUES(?,110010000,1313,1512,568,0)",player);
			String completed = quests.stream().map(q -> Integer.toString(q.getId())).collect(java.util.stream.Collectors.joining(","));
			update(c,"INSERT INTO poeta_journey(player_id,decision,chosen_class,start_exp,quest_expands,completed_quests,welcome_pending,journey_version) VALUES(?,'SKIP',?,?,?,?,1,2) ON DUPLICATE KEY UPDATE decision='SKIP',chosen_class=VALUES(chosen_class),start_exp=VALUES(start_exp),quest_expands=VALUES(quest_expands),completed_quests=VALUES(completed_quests),welcome_pending=1,journey_version=2",player,advanced.name(),exp,expansions,completed);
			c.commit();
		} catch (SQLException | RuntimeException e) { c.rollback(); throw e; }
	}

	public static boolean recoverCommittedChoice(int player) {
		if (!ready) return false;
		try (Connection c = DatabaseFactory.getConnection()) { return recover(c,player); }
		catch (SQLException e) { throw new IllegalStateException("Could not recover committed journey",e); }
	}

	static boolean recover(Connection c,int player) throws SQLException {
		c.setAutoCommit(false);
		try (PreparedStatement s = c.prepareStatement("SELECT chosen_class,start_exp,quest_expands,completed_quests FROM poeta_journey WHERE player_id=? AND decision='SKIP' AND needs_recovery=1 FOR UPDATE")) {
			s.setInt(1,player);
			try (ResultSet r = s.executeQuery()) {
				if (!r.next()) { c.rollback(); return false; }
				PlayerClass advanced = PlayerClass.valueOf(r.getString(1));
				update(c,"UPDATE players SET player_class=?,exp=GREATEST(exp,?),quest_expands=GREATEST(quest_expands,?),world_id=110010000,world_owner=0,x=1313,y=1512,z=568,heading=0 WHERE id=?",advanced.name(),r.getLong(2),r.getInt(3),player);
				for (String quest : r.getString(4).split(",")) if (!quest.isBlank()) {
					int id = Integer.parseInt(quest); if (id == 1007) saveCeremony(c,player,advanced); else saveQuest(c,player,id,"COMPLETE",0);
				}
				saveCeremony(c,player,advanced);
				saveQuest(c,player,PoetaJourneyRules.dispatchQuest(advanced),"START",0);
				update(c,"UPDATE poeta_journey SET needs_recovery=0 WHERE player_id=?",player);
				c.commit(); return true;
			}
		} catch (SQLException | RuntimeException e) { c.rollback(); throw e; }
	}

	/** Upgrades earlier skips once, before characters are loaded. Never creates mail. */
	static void migrate(Connection c) throws SQLException {
		for (String column : List.of("welcome_pending","journey_version")) {
			try (ResultSet r = c.getMetaData().getColumns(c.getCatalog(),null,"poeta_journey",column)) {
				if (r.next()) continue;
			}
			update(c,"ALTER TABLE poeta_journey ADD COLUMN "+column+(column.equals("welcome_pending") ? " BOOLEAN NOT NULL DEFAULT 0" : " INT NOT NULL DEFAULT 1"));
		}
		boolean previousAutoCommit=c.getAutoCommit();
		c.setAutoCommit(false);
		try {
			update(c,"UPDATE player_quests q JOIN poeta_journey j ON j.player_id=q.player_id SET q.status='START',q.quest_vars=1,q.flags=0,q.complete_count=0,q.complete_time=NULL WHERE q.quest_id=1007 AND j.decision='SKIP' AND j.journey_version<2 AND FIND_IN_SET('1007',j.completed_quests)>0");
			update(c,"UPDATE poeta_journey SET welcome_pending=1,journey_version=2 WHERE decision='SKIP' AND journey_version<2");
			c.commit();
		} catch (SQLException e) { c.rollback(); throw e; }
		finally { c.setAutoCommit(previousAutoCommit); }
	}

	static void saveCeremony(Connection c,int player,PlayerClass advanced) throws SQLException {
		try (PreparedStatement s=c.prepareStatement("SELECT complete_count FROM player_quests WHERE player_id=? AND quest_id=1007")) {
			s.setInt(1,player); try (ResultSet r=s.executeQuery()) { if (r.next() && r.getInt(1)>0) return; }
		}
		saveQuest(c,player,1007,"START",PoetaJourneyRules.ceremonyGroup(advanced));
		update(c,"UPDATE player_quests SET quest_vars=1,complete_count=0,complete_time=NULL WHERE player_id=? AND quest_id=1007",player);
	}

	static boolean welcomePending(Connection c,int player) throws SQLException {
		try (PreparedStatement s = c.prepareStatement("SELECT welcome_pending FROM poeta_journey WHERE player_id=? AND decision='SKIP'")) {
			s.setInt(1,player); try (ResultSet r=s.executeQuery()) { return r.next() && r.getBoolean(1); }
		}
	}

	static void acknowledge(Connection c,int player) throws SQLException {
		update(c,"UPDATE poeta_journey SET welcome_pending=0 WHERE player_id=? AND decision='SKIP'",player);
	}

	public static boolean ceremonyRewardsMailed(Player player) {
		// Only legacy receipts include 1007; new skips earn its rewards at turn-in.
		if (!ready) return false;
		try (Connection c=DatabaseFactory.getConnection(); PreparedStatement s=c.prepareStatement("SELECT 1 FROM poeta_journey WHERE player_id=? AND decision='SKIP' AND FIND_IN_SET('1007',completed_quests)>0")) {
			s.setInt(1,player.getObjectId()); try (ResultSet r=s.executeQuery()) { return r.next(); }
		} catch (SQLException e) { throw new IllegalStateException("Could not verify mailed ceremony rewards",e); }
	}

	private static void mail(Connection c,int player,int id,int item,long kinah) throws SQLException {
		update(c,"INSERT INTO mail(mail_unique_id,mail_recipient_id,sender_name,mail_title,mail_message,unread,attached_item_id,attached_kinah_count,express) VALUES(?,?,'Poeta Journey','Poeta quest rewards',?,1,?,?,0)",
			id,player,"Your rewards from the skipped Poeta quests and Ascension. All alternative reward items are included once. A Ceremony in Sanctum remains active: speak to Leah and earn its rewards when you complete it, then see Polyidus for Dispatch to Verteron.",item,kinah);
	}

	private static Object[] itemFields(int player,Item item) {
		return new Object[] {item.getObjectId(),item.getItemTemplate().getTemplateId(),item.getItemCount(),item.getItemColor(),item.getColorExpireTime(),item.getItemCreator(),item.getExpireTime(),item.getActivationCount(),player,false,item.isSoulBound(),0L,StorageType.MAILBOX.getId(),item.getEnchantLevel(),item.getEnchantBonus(),item.getItemSkinTemplate().getTemplateId(),item.getFusionedItemId(),item.getOptionalSockets(),item.getFusionedItemOptionalSockets(),item.getChargePoints(),item.getTuneCount(),item.getBonusStatsId(),item.getFusionedItemBonusStatsId(),item.getTempering(),item.getPackCount(),item.isAmplified(),item.getBuffSkill(),item.getRndPlumeBonusValue(),item.getRankLimitExpireTime()};
	}

	static int update(Connection c,String sql,Object... args) throws SQLException {
		try (PreparedStatement s = c.prepareStatement(sql)) { for (int i=0;i<args.length;i++) s.setObject(i+1,args[i]); return s.executeUpdate(); }
	}
}
