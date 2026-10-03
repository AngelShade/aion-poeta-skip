# Aion 4.8 Poeta Skip

An optional full-screen journey choice for the English Aion 4.8 NA 64-bit client and Beyond Aion emulator. Choose **Play Poeta** to keep the original story, or **Ascend to Sanctum** to choose an advanced class, start at level 10, complete the eligible Poeta quests and receive their items through mail.

The Sanctum ceremony stays playable: the skip passes Pernos and starts **A Ceremony in Sanctum** with **Leah**. Ceremony rewards are earned normally at its final turn-in. **Dispatch to Verteron** also starts for the chosen class. Existing completed quests never award another bundle.

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
- `D:\Poeta-Staged-server-media`: `poeta.jpg` and `sanctum.jpg`, generated from `Textures/loading/loading_lf1.dds` and `loading_lc1.dds` in your client.

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
4. Copy `game-server/config/journey` to the active server's `config/journey`. Copy the two generated JPG files from `Poeta-Staged-server-media` into its `config/journey/media`.
5. Deploy the modified `game-server/data/handlers/quest/poeta/_1000Prologue.java` into the matching active `data/handlers/quest/poeta` folder. Deploy the matching server data/configuration when building a fresh server. Customized emulators should merge the listed integration changes, preserving their own unrelated work.
6. Add these overrides to the active `config/mygs.properties`:

   ```properties
   gameserver.poeta.journey.enable = true
   gameserver.simple.secondclass.enable = false
   ```

7. Start GameServer and check **Poeta journey ready: 41 quests** and **Poeta journey listening at http://127.0.0.1:8091/journey**. Start the matching modified 64-bit client and log in.

The feature defaults to disabled until the operator installs both parts. Startup creates its InnoDB decision table and validates the persistence tables and reward templates. An unauthenticated `/journey/state` returning **403** is expected.

## Player behavior

New Elyos starting-class characters in Poeta, level 1-9, receive the automatic choice. Play saves their preference and starts the original prologue. Skip requires a compatible advanced class and explicit final confirmation while standing safely. It completes 41 eligible Poeta/Ascension quests, mails all fixed and alternative item rewards for the skipped quests, includes quest Kinah, titles and quest cube expansion, and binds/teleports to Sanctum.

The reward bundle includes other class alternatives; identical alternatives within one quest are awarded once. The skip grants level 10 rather than adding scaled XP for each quest. Restricted, event, unused and repeatable quests are excluded. Previously ascended or transferred characters cannot claim the skip. A full mailbox rejects the whole transaction, and repeated requests cannot duplicate rewards.

The welcome screen survives map entry and stays until **Enter the world** is clicked. Reopen through **Additional Functions > Choose Your Journey** or `/journey`. Complete the ceremony in Sanctum and continue to Verteron through Polyidus. Legacy receipts that already mailed ceremony rewards retain their protection against receiving them twice.

## Verification and rollback

See [validation](docs/VALIDATION.md) for tested behavior and remaining recipient checks. Test Play and Skip on separate new Elyos characters, actual button hit areas at your resolution/UI scale, completed quest history, Leah's ceremony step, rewards/mail, persistent welcome dismissal, relogin, a summoned pet and Additional Functions.

To restore the client, fully exit Aion and use the exact backup printed by installation:

```powershell
./client-mods/poeta-journey/Restore.ps1 -ClientPath 'D:\Games\Aion 4.8 NA' -BackupPath 'D:\Games\Aion 4.8 NA\TransmogMenu-backups\signed-DATE-ID'
```

To disable the server feature, set `gameserver.poeta.journey.enable = false` and restart normally. Keep `poeta_journey`: its receipts prevent duplicate claims. Restoring client files or disabling the feature does not reverse awarded levels, quests, items or mail. Restoring a database snapshot reverses subsequent gameplay too; use a consistent backup and reconcile rewards first.
