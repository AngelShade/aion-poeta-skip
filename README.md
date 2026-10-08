# Aion 4.8 Poeta & Ishalgen Skip

## Install with the other published Aetherfall mods

This mod is included in the [combined Season Pass release](https://github.com/AngelShade/aion-season-pass). Its [shared installation guide](https://github.com/AngelShade/aion-season-pass/blob/main/docs/SHARED_MODS.md) combines **Season Pass, Central Market, Wardrobe, Skip Poeta/Ishalgen Journey and Inventory/Warehouse Expansion** in one native client package and one server listener. Use that profile when installing these mods together; do not layer the separate standalone installers. Recorded standalone installations can be upgraded using a separate original client copy. Offline integration checks are distinct from actual gameplay acceptance.

An optional full-screen journey choice for the English Aion 4.8 NA 64-bit client and Beyond Aion emulator. Both **Elyos and Asmodians** are supported. Choose **Play Poeta / Play Ishalgen** to keep your original story, or **Ascend to Sanctum / Ascend to Pandaemonium** to choose an advanced class, start at level 10, complete your eligible starter quests and receive their items through mail.

Both capital ceremonies stay playable and grant their rewards normally at final turn-in. The skip bypasses Pernos or Munin and starts the appropriate ceremony at its first capital step. Existing completed quests never award another bundle.

| Faction | Starter quests completed | Capital ceremony begins with | Onward quest |
| --- | --- | --- | --- |
| Elyos | 41 Poeta/Ascension quests | Leah in Sanctum | Dispatch to Verteron, via Polyidus |
| Asmodians | 47 Ishalgen/Ascension quests | Heimdall in Pandaemonium | Dispatch to Altgard, via Doman |

## Download

Use **Code > Download ZIP** on [GitHub](https://github.com/AngelShade/aion-poeta-skip), or:

```powershell
git clone https://github.com/AngelShade/aion-poeta-skip.git
```

This is a complete server source repository based on [Beyond Aion](https://github.com/beyond-aion/aion-server), commit `267ce6033f39e8d297d2ac2657e5a6e930723578`. The upstream history, [original README](docs/UPSTREAM_README.md) and GPL-3.0 license are retained. The Poeta service has its own local HTTP listener; Central Market, Wardrobe and the private Cash Shop are not required.

No original client DLLs, signed client archives, extracted client artwork, compiled server JARs, database credentials or player data are distributed. The client builder compiles the native bridge locally and creates background images from the recipient's own loading textures. It adds only the Journey menu; enlarged Inventory, Warehouse, Graphics and other private menus are not installed.

## Requirements and supported setup

- JDK 25, Maven and MySQL/MariaDB for this server.
- Original English **Aion 4.8 NA 64-bit client**. Game.dll must have SHA-256 `5334cf2164468678e45fe1a5decf58a0fbc4fd7f22cfdcbb87d28edce8d2c11c`. CrySystem and Awesomium are also version checked. Other builds and already modified Game.dll files are refused by this standalone builder.
- Python 3.12, Pillow (`python -m pip install Pillow`) and Visual Studio 2022 C++ Build Tools with the Windows SDK for client preparation.
- Client and GameServer on the **same computer**: the supplied authenticated route is `http://127.0.0.1:8091/journey`. Remote hosting requires coordinated native URL validation, listener and transport changes; changing only the Lua URL is insufficient.
- Port 8091 must be free. If combining this with another mod using that port, register `/journey` on its existing HTTP listener and reuse its authentication integration; do not start two listeners on the same address. See [integration notes](docs/INTEGRATION.md).

## Prepare the client and artwork

From the downloaded repository root:

```powershell
python client-mods/poeta-journey/build_package.py --client-path 'D:\Games\Aion 4.8 NA' --java 'C:\Program Files\Java\jdk-25\bin\java.exe' --output 'D:\Poeta-Staged'
```

Choose the **game root containing bin64, Data, L10N and Plugin**, not bin64 itself. The output must be a new directory outside the client. Preparation leaves the client untouched and creates:

- `D:\Poeta-Staged`: ten hash-checked client replacements and `manifest.json`.
- `D:\Poeta-Staged-server-media`: `poeta.jpg`, `sanctum.jpg`, `ishalgen.jpg` and `pandaemonium.jpg`, generated from your client's `Textures/loading/loading_lf1.dds`, `loading_lc1.dds`, `loading_df1.dds` and `loading_dc1.dds`.

Review the manifest. Fully exit Aion before installing:

```powershell
./client-mods/poeta-journey/Install.ps1 -ClientPath 'D:\Games\Aion 4.8 NA' -PreparedPath 'D:\Poeta-Staged'
```

The installer verifies original and staged hashes, backs up every replacement and prints the backup path. It preserves the original model public key and isolates addon archive signatures so stock pet validation and the menu can work together. Existing item and inventory/UI archives are preserved. The package belongs to the exact client root used to prepare it.

## Build and deploy the server

1. Build from this repository root:

   ```powershell
   mvn -pl game-server -am clean package '-Dassembly.skipAssembly=true' '-Dmaven.test.skip=true'
   ```

2. Back up the database and your current server JAR/configuration. Build your own JAR instead of copying a private server's compiled binary.
3. Log players out and stop GameServer normally so its save completes. Replace `libs/game-server-4.8-SNAPSHOT.jar` in the **active GameServer deployment** with `game-server/target/game-server-4.8-SNAPSHOT.jar`.
4. Copy `game-server/config/journey` to the active server's `config/journey`. Copy all four generated JPG files from `Poeta-Staged-server-media` into its `config/journey/media`.
5. Deploy both modified prologue handlers: `game-server/data/handlers/quest/poeta/_1000Prologue.java` and `game-server/data/handlers/quest/ishalgen/_2000Prologue.java` into the matching active handler folders. Deploy the matching server data/configuration when building a fresh server. Customized emulators should merge the listed integration changes, preserving their own unrelated work.
6. Add these overrides to the active `config/mygs.properties`:

   ```properties
   gameserver.poeta.journey.enable = true
   gameserver.simple.secondclass.enable = false
   ```

7. Start GameServer and check **Starter journey ready: 41 Poeta quests, 47 Ishalgen quests** and **Poeta journey listening at http://127.0.0.1:8091/journey**. Start the matching modified 64-bit client and log in.

The feature defaults to disabled until the operator installs both parts. Startup creates its InnoDB decision table and validates the persistence tables and reward templates. An unauthenticated `/journey/state` returning **403** is expected.

## Updating the earlier Elyos-only version

Replace the built server JAR through the normal stop/save path, deploy both prologue handlers and updated `config/journey/media` HTML/JS, and add `ishalgen.jpg` and `pandaemonium.jpg` from a newly prepared package. An already installed matching Journey client uses the same native browser hooks and does not need another DLL install for faction support. Preserve the existing `poeta_journey` table and receipts.

## Player behavior

New Elyos characters in Poeta and Asmodian characters in Ishalgen, starting class and level 1-9, receive the automatic choice. Play saves their preference and starts the original prologue. Skip requires a compatible advanced class and explicit final confirmation while standing safely. It completes the 41 eligible Poeta/Ascension quests or 47 eligible Ishalgen/Ascension quests for that faction, mails all fixed and alternative item rewards, includes quest Kinah, titles and quest cube expansion, and binds/teleports to the matching capital. Other-faction quests and rewards are never included.

The reward bundle includes other class alternatives; identical alternatives within one quest are awarded once. The skip grants level 10 rather than adding scaled XP for each quest. Restricted, event, unused and repeatable quests are excluded. Previously ascended or transferred characters cannot claim the skip. A full mailbox rejects the whole transaction, and repeated requests cannot duplicate rewards.

The welcome screen survives map entry and stays until **Enter the world** is clicked. Reopen through **Additional Functions > Choose Your Journey** or `/journey`. Elyos complete the ceremony with Leah and continue to Verteron through Polyidus. Asmodians begin their ceremony with Heimdall and continue to Altgard through Doman. Legacy receipts that already mailed ceremony rewards retain their protection against receiving them twice.

## Verification and rollback

See [validation](docs/VALIDATION.md) for tested behavior and remaining recipient checks. Test Play and Skip on separate new characters of both factions, actual button hit areas at your resolution/UI scale, completed quest history, Leah/Heimdall ceremony steps, rewards/mail, persistent welcome dismissal, relogin, a summoned pet and Additional Functions.

To restore the client, fully exit Aion and use the exact backup printed by installation:

```powershell
./client-mods/poeta-journey/Restore.ps1 -ClientPath 'D:\Games\Aion 4.8 NA' -BackupPath 'D:\Games\Aion 4.8 NA\TransmogMenu-backups\signed-DATE-ID'
```

To disable the server feature, set `gameserver.poeta.journey.enable = false` and restart normally. Keep `poeta_journey`: this table name is retained for compatibility and its receipts protect both factions from duplicate claims. Restoring client files or disabling the feature does not reverse awarded levels, quests, items or mail. Restoring a database snapshot reverses subsequent gameplay too; use a consistent backup and reconcile rewards first.
