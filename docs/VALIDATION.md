# Validation

The standalone public source and client tools were checked on 2026-10-03 with JDK 25, Python 3.12, MSVC 2022 and the matching English Aion 4.8 NA browser engine. This publication did not install or restart the live game/server.

## Passed

- Maven reactor package build: Commons and all 2,306 GameServer source files compiled successfully with the upstream dependency versions. Assembly was skipped; verification programs below were run separately.
- Quest rules: 41 eligible Poeta/Ascension and 47 Ishalgen/Ascension quests; both complete faction rosters; 92 unique reward item templates; all eleven advanced classes and six class-specific dispatches per faction; both ceremony rewards excluded from early mail.
- 164 database checks using the actual published transaction/recovery methods in a disposable empty schema: completed quest status/counts, Leah/Heimdall ceremony steps, reward attachments, atomic rollback, duplicate clicks, mailbox limits and crash recovery. The additional Asmodian cases exercise all eleven advanced classes, reject the Elyos quest roster, verify Pandaemonium teleport/bind and Altgard dispatch, persist faction-specific mail, and recover without sending rewards again. Existing player rows were not changed.
- 13 HTTP handler checks on a disposable loopback port: actual menu and all four faction artwork assets, valid methods/paths/Host, unauthenticated state/actions rejected.
- Clean matching DLL/addon fixture produced ten client replacements and all four server artwork files. The original native DLL hooks were unchanged by faction support. Stock archive signatures were verified before signing; original model key retained; native bridge and item index built locally. Existing item/UI archives were preserved.
- 100 native viewport/UI-scale cases, including resize, zero Journey inset, other-widget fallback and invalid viewport handling.
- Generated native browser machine code: account-token encoding, exact Journey URL matching, missing-token request, publisher fallback and permitted DLL modification bounds.
- Both faction menus in actual Aion Awesomium/WebKit at 1024x768, 1920x1080 and 3440x1440: physical mouse clicks matched visible buttons; class confirmation worked; welcome survived map reload and waited for acknowledgement; Play closed the menu; completed/ineligible characters did not automatically open it.
- Native bridge callbacks in both initialization orders: Journey visibility registered while stock callbacks remained available. Original DDS decoding, callback reuse and remote-command refusal passed.
- Installer refused while the live game was running. Disposable fixture install/restore passed: all ten replacement hashes matched, preserved archives remained unchanged, restoration matched every original and removed added files. Only test-local copies replaced the global running-game check with an exact fixture-path assertion; published scripts retain their close-Aion guard.
- Diff whitespace and added-file inspection passed; generated DLLs, client archives, item indexes, signing keys, artwork and private deployment data are excluded.

## Reproduce

Build as described in the main README. For the standalone Java checks, first compile the check classes with a classpath containing `game-server/target/classes`, `commons/target/classes` and the GameServer dependency JARs. Run `PoetaJourneyRulesCheck` from the repository root with the matching `game-server/data/static_data/quest_data/quest_data.xml` path.

`PoetaJourneyDatabaseCheck` runs from the repository root and takes an absolute GameServer deployment directory. It reads its database configuration, creates a uniquely named empty test schema using the existing table structures, and drops that schema afterwards. The database user needs CREATE/DROP DATABASE rights. It does not insert/update live player rows.

`PoetaJourneyHttpCheck` runs with `game-server` as its working directory after generating/copying all four local JPG assets. It uses an ephemeral local port and no character session.

```powershell
python client-mods/poeta-journey/tests/verify_market_viewport.py --original-dll 'D:\Games\Aion 4.8 NA\bin64\Game.dll'
python client-mods/poeta-journey/tests/verify_market_browser.py --original-dll 'D:\Games\Aion 4.8 NA\bin64\Game.dll'
python client-mods/poeta-journey/tests/verify_journey_browser.py --browser-bin 'D:\Games\Aion 4.8 NA\bin64'
```

The first two commands require an untouched matching Game.dll, before installation. The last command requires the locally generated JPG assets in `game-server/config/journey/media`; it runs a separate browser test process and writes test screenshots locally. The inherited `market` filenames refer to the shared browser hook generator, which also implements Journey.

## Recipient acceptance still required

The standalone published server was not launched beside the live deployment. A complete in-game test of this independent package is still required on the recipient's installation: fresh login, Play and Skip on separate new characters of both factions, class/level/quest history, attachment collection, Leah/Heimdall ceremonies and normal rewards, onward travel, persistent welcome dismissal, relogin, a summoned pet and Additional Functions.

These automated checks validate source, transactions and the actual browser engine; they do not establish compatibility with different clients, custom emulator schemas, merged mods or remote hosting. Use the exact supported original DLLs, follow the integration notes when combining mods, and retain the installer backup.
