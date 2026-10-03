# Integrating into another emulator

This branch starts from the original Beyond Aion 4.8 source rather than the author's private server. Review the diff against upstream `267ce6033f39e8d297d2ac2657e5a6e930723578`.

Server changes: `PoetaJourneyRules`, `PoetaJourneyService`, `PoetaJourneyHttpService`, the CustomConfig flag, QuestTemplate's quest_zone attribute/getter, the Poeta and Ishalgen prologue gates, login recovery in PlayerService, legacy ceremony reward protection in QuestService, and GameServer start/ShutdownHook stop integration. `config/journey` contains the schema and ES5 browser media. Do not replace customized QuestService, PlayerService, GameServer or configuration files wholesale.

The standalone listener on 127.0.0.1:8091 resolves an online player from the same native account security token used by the other browser mods. A per-connection, expiring, single-use form receipt authorizes actions. Host, origin and loopback checks remain in effect.

When adding the route to an existing listener on that same address, remove the standalone start/stop calls, call PoetaJourneyService.start() once after database/static data initialization, and register `/journey` with PoetaJourneyHttpService::handle. Keep player token resolution and receipt checks. All routes remain local with this build.

The clean-client builder is intentionally separate from the author's incremental private-client updater. Combining client mods requires merging their addon XML/Lua, native routes, DLL hooks, signing and restore tracking in one version-checked package. Installing two independently prepared packages over one another is unsupported.

The JPG files are generated locally, not hosted in this repository. The operator must copy the generated server-media files to the active deployment before enabling the menu.

Faction selection comes from the online character and the persisted player race, never a browser parameter. Teleport/bind location, quest roster, ceremony ID/group, dispatch and recovery all use the same faction profile. Existing `poeta_journey` receipts and the server enable flag are retained; no new table or forced database reset is needed. Existing Journey-enabled clients can render the new faction menu from updated server media; include both new Asmodian JPG files when deploying.
