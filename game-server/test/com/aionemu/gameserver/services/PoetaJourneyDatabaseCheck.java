package com.aionemu.gameserver.services;

import java.nio.file.*;
import java.sql.*;
import java.util.*;
import javax.xml.bind.JAXBContext;
import com.aionemu.gameserver.dataholders.QuestsData;
import com.aionemu.gameserver.model.PlayerClass;
import com.aionemu.gameserver.model.Race;
import com.aionemu.gameserver.model.templates.QuestTemplate;

/** Exercises the actual commit/recovery methods in a disposable empty schema. */
public final class PoetaJourneyDatabaseCheck {
	private static int checks;
	private static void check(boolean pass,String label) { checks++; if (!pass) throw new AssertionError(label); }
	private static long value(Connection c,String sql) throws SQLException {
		try (Statement s=c.createStatement(); ResultSet r=s.executeQuery(sql)) { r.next(); return r.getLong(1); }
	}
	private static void player(Connection c,int id) throws SQLException {
		player(c,id,Race.ELYOS,PlayerClass.WARRIOR);
	}
	private static void player(Connection c,int id,Race race,PlayerClass base) throws SQLException {
		PoetaJourneyService.update(c,"INSERT INTO players(id,name,account_id,account_name,x,y,z,heading,world_id,gender,race,player_class) VALUES(?, ?,1,'fixture',1,2,3,0,?,'FEMALE',?,?)",id,"Fixture"+id,PoetaJourneyRules.journey(race).starterWorld(),race.name(),base.name());
	}
	private static Object[] item(int id,int player) {
		Object[] v=new Object[29]; Arrays.fill(v,0);v[0]=id;v[1]=162001057;v[2]=5L;v[3]=null;v[5]=null;v[8]=player;v[12]=7;v[15]=162001057; return v;
	}
	public static void main(String[] args) throws Exception {
		Properties p=new Properties(); Path deploy=Path.of(args[0]);
		try(var in=Files.newInputStream(deploy.resolve("config/network/database.properties"))) { p.load(in); }
		try(var in=Files.newInputStream(deploy.resolve("config/mygs.properties"))) { p.load(in); }
		String url=p.getProperty("database.url").replace("${gameserver.timezone}","UTC"),user=p.getProperty("database.user"),password=p.getProperty("database.password");
		String schema="aion_journey_check_"+System.currentTimeMillis();check(schema.matches("aion_journey_check_[0-9]+"),"isolated schema name");
		QuestsData data=(QuestsData) JAXBContext.newInstance(QuestsData.class).createUnmarshaller().unmarshal(Path.of("game-server/data/static_data/quest_data/quest_data.xml").toFile());
		List<QuestTemplate> quests=data.getQuestTemplates().stream().filter(q -> PoetaJourneyRules.skippedQuest(q,Race.ELYOS)).toList();
		try(Connection live=DriverManager.getConnection(url,user,password);Statement s=live.createStatement()) {
			String source=live.getCatalog();check(source.matches("[A-Za-z0-9_]+")&&!source.equals(schema),"source remains read-only");
			s.execute("CREATE DATABASE `"+schema+"`");
			try {
				for(String table:List.of("players","player_quests","player_titles","player_bind_point","inventory","mail")) s.execute("CREATE TABLE `"+schema+"`."+table+" LIKE `"+source+"`."+table);
				try(Connection c=DriverManager.getConnection(url.replace("/"+source,"/"+schema),user,password);Statement ddl=c.createStatement()) {
					check(c.getCatalog().equals(schema),"test uses its own schema");
					ddl.execute(Files.readString(Path.of("game-server/config/journey/schema.sql")));
					player(c,101);player(c,102);player(c,103);
					var mails=List.of(new PoetaJourneyService.MailAward(1001,item(2001,101),0),new PoetaJourneyService.MailAward(1002,null,250000));
					PoetaJourneyService.commitSkip(c,101,PlayerClass.GLADIATOR,100000,1,quests,Set.of(1,4,7),mails);
					check(value(c,"SELECT COUNT(*) FROM player_quests WHERE player_id=101 AND status='COMPLETE'")==41,"41 Poeta quests completed; Sanctum ceremony remains active");
					check(value(c,"SELECT quest_vars FROM player_quests WHERE player_id=101 AND quest_id=1007 AND status='START'")==1,"ceremony starts with Leah after Pernos teleport");
					check(PoetaJourneyService.welcomePending(c,101),"welcome survives map entry");
					PoetaJourneyService.acknowledge(c,101);c.commit();
					check(!PoetaJourneyService.welcomePending(c,101),"welcome closes only after acknowledgement");
					check(value(c,"SELECT reward FROM player_quests WHERE player_id=101 AND quest_id=1007")==0,"warrior ceremony group");
					check(value(c,"SELECT FIND_IN_SET('1007',completed_quests) FROM poeta_journey WHERE player_id=101")==0,"new skip receipt allows ceremony rewards at turn-in");
					check(value(c,"SELECT COUNT(*) FROM player_quests WHERE player_id=101 AND quest_id=1913 AND status='START'")==1,"onward quest active");
					check(value(c,"SELECT COUNT(*) FROM mail WHERE mail_recipient_id=101")==2,"item and currency mails");
					check(value(c,"SELECT COUNT(*) FROM inventory WHERE item_owner=101 AND item_location=7")==1,"mail attachment persisted");
					check(value(c,"SELECT SUM(attached_kinah_count) FROM mail WHERE mail_recipient_id=101")==250000,"mailed currency");
					check(value(c,"SELECT world_id FROM players WHERE id=101")==110010000,"Sanctum destination");
					check(value(c,"SELECT map_id FROM player_bind_point WHERE player_id=101")==110010000,"Sanctum return point");
					check(value(c,"SELECT COUNT(*) FROM player_titles WHERE player_id=101")==3,"quest titles preserved");
					boolean duplicate=false;try { PoetaJourneyService.commitSkip(c,101,PlayerClass.GLADIATOR,100000,1,quests,Set.of(1),mails); }catch(IllegalArgumentException e){duplicate=true;}
					check(duplicate && value(c,"SELECT COUNT(*) FROM mail WHERE mail_recipient_id=101")==2,"repeated click cannot duplicate rewards");
					var broken=List.of(new PoetaJourneyService.MailAward(1003,item(2002,102),0),new PoetaJourneyService.MailAward(1003,item(2003,102),0));
					boolean failed=false;try { PoetaJourneyService.commitSkip(c,102,PlayerClass.TEMPLAR,100000,1,quests,Set.of(1),broken); }catch(SQLException e){failed=true;}
					check(failed,"mid-bundle failure simulated");
					check(value(c,"SELECT COUNT(*) FROM inventory WHERE item_owner=102")==0 && value(c,"SELECT COUNT(*) FROM mail WHERE mail_recipient_id=102")==0,"failed attachments and letters rolled back");
					check(value(c,"SELECT COUNT(*) FROM player_quests WHERE player_id=102")==0 && value(c,"SELECT COUNT(*) FROM poeta_journey WHERE player_id=102")==0,"failed quest completion and decision rolled back");
					check(value(c,"SELECT world_id FROM players WHERE id=102")==210010000,"failed skip preserves starting position");
					for(int i=0;i<200;i++) PoetaJourneyService.update(c,"INSERT INTO mail(mail_unique_id,mail_recipient_id,sender_name,mail_title,mail_message,unread,attached_item_id,attached_kinah_count,express) VALUES(?,103,'fixture','fixture','fixture',1,0,0,0)",3000+i);c.commit();
					boolean full=false;try { PoetaJourneyService.commitSkip(c,103,PlayerClass.GLADIATOR,100000,1,quests,Set.of(),List.of(new PoetaJourneyService.MailAward(4001,null,1))); }catch(IllegalArgumentException e){full=true;}
					check(full && value(c,"SELECT COUNT(*) FROM player_quests WHERE player_id=103")==0,"full mailbox refuses the whole skip");
					PoetaJourneyService.update(c,"UPDATE players SET player_class='WARRIOR',exp=0,quest_expands=0,world_id=210010000 WHERE id=101");
					PoetaJourneyService.update(c,"UPDATE player_quests SET status='START',complete_count=0 WHERE player_id=101 AND quest_id=1007");
					PoetaJourneyService.update(c,"UPDATE poeta_journey SET needs_recovery=1 WHERE player_id=101");c.commit();
					check(PoetaJourneyService.recover(c,101),"post-commit refresh failure recovered");
					check(value(c,"SELECT exp FROM players WHERE id=101")==100000 && value(c,"SELECT world_id FROM players WHERE id=101")==110010000,"saved class progression and position restored");
					check(value(c,"SELECT complete_count FROM player_quests WHERE player_id=101 AND quest_id=1007")==0,"active ceremony is not in completed journal");
					check(value(c,"SELECT COUNT(*) FROM mail WHERE mail_recipient_id=101")==2,"recovery does not remail rewards");
					check(!PoetaJourneyService.recover(c,101),"recovery is one-shot");
					PoetaJourneyService.update(c,"UPDATE player_quests SET status='COMPLETE',quest_vars=0,complete_count=1 WHERE player_id=101 AND quest_id=1007");
					PoetaJourneyService.saveCeremony(c,101,PlayerClass.GLADIATOR);c.commit();
					check(value(c,"SELECT COUNT(*) FROM player_quests WHERE player_id=101 AND quest_id=1007 AND status='COMPLETE'")==1,"completed ceremony is not reopened or rewarded twice");
					PoetaJourneyService.update(c,"UPDATE poeta_journey SET completed_quests=CONCAT(completed_quests,',1007') WHERE player_id=101");
					check(value(c,"SELECT FIND_IN_SET('1007',completed_quests) FROM poeta_journey WHERE player_id=101")>0,"legacy mailed ceremony receipt remains distinguishable");
					PoetaJourneyService.update(c,"ALTER TABLE poeta_journey DROP COLUMN welcome_pending, DROP COLUMN journey_version");c.commit();
					PoetaJourneyService.migrate(c);
					check(value(c,"SELECT quest_vars FROM player_quests WHERE player_id=101 AND quest_id=1007 AND status='START'")==1,"earlier skip repaired to Sanctum step");
					check(PoetaJourneyService.welcomePending(c,101)&&value(c,"SELECT COUNT(*) FROM mail WHERE mail_recipient_id=101")==2,"repair opens welcome without remailing");
					PoetaJourneyService.update(c,"UPDATE player_quests SET status='COMPLETE',quest_vars=0,complete_count=1 WHERE player_id=101 AND quest_id=1007");c.commit();
					PoetaJourneyService.migrate(c);
					check(value(c,"SELECT COUNT(*) FROM player_quests WHERE player_id=101 AND quest_id=1007 AND status='COMPLETE'")==1,"later restarts preserve finished ceremony");
					List<QuestTemplate> asmodian=data.getQuestTemplates().stream().filter(q -> PoetaJourneyRules.skippedQuest(q,Race.ASMODIANS)).toList();
					int fixture=500;
					for(PlayerClass advanced:PlayerClass.values()) if(!advanced.isStartingClass()) {
						int id=fixture++;player(c,id,Race.ASMODIANS,advanced.getStartingClass());c.commit();
						boolean wrongRace=false;
						try { PoetaJourneyService.commitSkip(c,id,advanced,100000,1,quests,Set.of(),List.of()); }
						catch(IllegalArgumentException e) { wrongRace=true; }
						check(wrongRace && value(c,"SELECT COUNT(*) FROM player_quests WHERE player_id="+id)==0,"wrong faction transaction refused "+advanced);
						var letters=List.of(new PoetaJourneyService.MailAward(10000+id,item(20000+id,id),0),new PoetaJourneyService.MailAward(30000+id,null,100));
						PoetaJourneyService.commitSkip(c,id,advanced,100000,1,asmodian,Set.of(),letters);
						check(value(c,"SELECT COUNT(*) FROM player_quests WHERE player_id="+id+" AND status='COMPLETE'")==47,"47 Ishalgen quests completed "+advanced);
						check(value(c,"SELECT quest_vars FROM player_quests WHERE player_id="+id+" AND quest_id=2009 AND status='START' AND complete_count=0")==1,"ceremony begins with Heimdall after Munin "+advanced);
						check(value(c,"SELECT reward FROM player_quests WHERE player_id="+id+" AND quest_id=2009")==PoetaJourneyRules.ceremonyGroup(advanced),"Asmodian ceremony class group "+advanced);
						check(value(c,"SELECT COUNT(*) FROM player_quests WHERE player_id="+id+" AND quest_id="+PoetaJourneyRules.dispatchQuest(Race.ASMODIANS,advanced)+" AND status='START'")==1,"Altgard dispatch "+advanced);
						check(value(c,"SELECT COUNT(*) FROM player_quests WHERE player_id="+id+" AND quest_id IN (1000,1006,1007,1913)")==0,"no Elyos progression "+advanced);
						check(value(c,"SELECT world_id FROM players WHERE id="+id)==120010000 && value(c,"SELECT map_id FROM player_bind_point WHERE player_id="+id)==120010000,"Pandaemonium teleport and bind "+advanced);
						check(value(c,"SELECT COUNT(*) FROM mail WHERE mail_recipient_id="+id+" AND mail_title='Ishalgen rewards' AND mail_message LIKE '%Heimdall%' AND mail_message LIKE '%Altgard%'")==2,"Asmodian reward mail and directions "+advanced);
						check(value(c,"SELECT COUNT(*) FROM inventory WHERE item_owner="+id+" AND item_location=7")==1 && value(c,"SELECT FIND_IN_SET('2009',completed_quests) FROM poeta_journey WHERE player_id="+id)==0,"attachment persisted and ceremony rewards deferred "+advanced);
						PoetaJourneyService.update(c,"UPDATE players SET world_id=220010000,exp=0 WHERE id=?",id);
						PoetaJourneyService.update(c,"UPDATE poeta_journey SET needs_recovery=1 WHERE player_id=?",id);c.commit();
						check(PoetaJourneyService.recover(c,id) && value(c,"SELECT world_id FROM players WHERE id="+id)==120010000,"Asmodian recovery returns to correct capital "+advanced);
						check(value(c,"SELECT COUNT(*) FROM mail WHERE mail_recipient_id="+id)==2 && !PoetaJourneyService.recover(c,id),"Asmodian recovery never remails "+advanced);
						boolean repeated=false;try { PoetaJourneyService.commitSkip(c,id,advanced,100000,1,asmodian,Set.of(),letters); } catch(IllegalArgumentException e) { repeated=true; }
						check(repeated && value(c,"SELECT COUNT(*) FROM mail WHERE mail_recipient_id="+id)==2,"Asmodian duplicate claim blocked "+advanced);
					}
				}
			} finally { s.execute("DROP DATABASE `"+schema+"`"); }
		}
		System.out.println("PASS: "+checks+" isolated database checks: atomic rewards/completion, all-or-nothing rollback, duplicate clicks, mailbox capacity, and crash recovery.");
	}
}
