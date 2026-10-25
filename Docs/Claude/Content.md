# Content map, editor module, tools

## Content/
- `ZERO/`: **the game's own content.**
  - `Animation/Deadlock`: `ABP_DeadlockHero` (see Animation.md).
  - `BP/BaseGameplayAbility`: base ability/GE BPs (melee, parry, stun, reload, knockback, `GE_DamageBase`).
  - `BP/Character`: `BP_ZERO_BaseCharacter`. `BP/GameMode`: `BP_TDMGameMOde`, `BP_Zero_PlayerStart`. `BP/ITEMS`: `BP_Zero_Item_Data_Master`, `DT_ItemList`, `Vitality/`.
  - `Core`: `BP_ZeroGameInstance`, `ZeroBaseGameMode`, `BP_BaseZiplineActor`, `AN_TriggerEventNotify`.
  - `monkeyBoi/`: `BP_Mover_Monkey` (reference Mover pawn: modes + transitions configured here), abilities, montages. `Moveer/`: `BP_ZeromoverPawn`, Mover test game mode and movement settings.
  - `Bow_Bitch`, `GuNGuy`: older per-hero folders. `DeadlockChar/Pocket`: Pocket built from the Deadlock import.
  - `HeroAbilities/{ApolloTest,DoormanTest,DrifterTest,LashTesst,PocketTest,ViscousTest}/<Ability>/BP_<Hero>_<Ability>`: hero ability BPs. Each hero folder also holds its curve table `C_<Hero>` (ability values per level) and, for the four Deadlock heroes, `GE_<Hero>_DefaultStats` (the hero BP's `DefaultGameplayEffect`; copies of `Developers/rpm86/GAS_TEST/GE_Defaulttest`, made 2026-10-22).
  - `Levels`: `CharacterSelector.umap`, `Lvl_MainMenu.umap`. `MATS`: materials.
  - `UI/BP/...`: widget BPs (`Widgets/WBP_BaseUILayout`, `BP_BAseHUD`, `WBP_PlayerHUD`, `CharacterSelector/`, `DamageNumber/`, `Input/`, `OverHead/`, `Widgets/ItemShop`, `MainMenu`, `Abilities`, `StatBoxs`; `UI/BP/Weapon` holds fire-ability BPs, misplaced). `UI/CommonStyles`, `UI/CommonUI/Inputs`, `UI/Images`, `UI/Materials`.
- `Developers/rpm86/`: personal sandbox (test GEs, editor utility widgets `EUW_*`, `EOSTEST`, `GAS_TEST`).
  - **`Developers/rpm86/Deadlock/{Apollo,Lash,Pocket,_Shared}`: imported Valve Deadlock assets. Local prototyping only. Never ship, never reference from shipping content, keep out of shared branches.**
  - `BP_Deadlock_Lash` / `BP_Deadlock_Apollo` / `BP_Deadlock_Pocket` / `TheDoorman/BP_Deadlock_TheDoorman`: Mover pawns using `ABP_DeadlockHero`. Doorman imports as `TheDoorman` (asset names come from the display name); it was duplicated from Pocket; all four slots now have Doorman abilities (Heroes.md). Its anim set has no hard_land, wall_jump or run_aim clip.
- `Developers/Levi`: another dev's sandbox.
- Marketplace/template: `ParagonSparrow`, `ParagonSunWukong`, `ParagonTwinblast`, `Characters/Mannequin*`, `StarterContent`, `ThirdPerson`, `LevelPrototyping`, `UIMaterialLab`, `Blueprints` (template leftovers).
- `AbilityIcons/`, `Data/{Images,Items,ShopImages}`, `Scripts/response.json` (item API dump for the image downloader).
- `__ExternalActors__`, `__ExternalObjects__`: one-file-per-actor data.

## Editor module (`Source/ZeroEditorModule`)
- `FZeroEditorModuleModule`: adds a "ZeroLock" menu to the Level Editor with "Hero Workbench" (opens the nomad tab `ZeroLockHeroWorkbench`) and "BaseAbilityList" (logs every `UBaseGameplayAbility` subclass + properties). Shutdown unregisters the tab and removes the menu extender.
- **Hero Workbench** (`Private/HeroWorkbench/`):
  - `SZL_HeroWorkbench` (Slate). Left: heroes from the game instance's `CharacterDataTable` (`DT_HeroList`, table order; fallback path `/Game/ZERO/UI/BP/CharacterSelector/DT_HeroList`), with search, Rescan, and "Show heroes not in the hero list" (every other `AZeroLockCharacter` Blueprint under /Game). Each row shows the icon and error/warning counts.
  - Right, pages for the selected hero:
    - **Overview**: data asset details and the hero's checks.
    - **Abilities**: one row per ability property on the hero class, found by reflection (`TSubclassOf<UGameplayAbility>` properties, then array elements such as `DefaultAbilities`, so new slots show up automatically). Shows cooldown, tags and issues; a slot-assignment panel; details of the selected ability's class defaults.
    - **Mesh & Anim**: the `MeshComponent` template, the anim set, and the melee montages.
    - **Weapon**: summary, primary fire ability (Weapon/Cooldown/Costs categories), projectile, and projectile movement speeds.
    - **Base Stats**: `DefaultGameplayEffect` modifiers at level 1, plus their editor. The Value column is editable in place for plain scalable-float modifiers (writes the effect CDO, then re-runs the checks); curve-table, attribute-based, custom-calculation and SetByCaller magnitudes stay read-only text. Says which other heroes share the effect; "Make a copy for this hero" / "...for every hero sharing it" duplicate it to `<hero ability folder>/GE_<Hero>_DefaultStats` (or reuse that asset) and assign it (`ZLHeroAudit::CopyDefaultEffectForHero`).
    - **Curve Table**: the hero's table (the one most of its ability `FScalableFloat`s use, else `C_<Hero>` in its folder). "Create C_<Hero>"; "Move constant values into the table" (`LinkHeroValuesToTable`: every non-zero constant `FScalableFloat` on the hero's own abilities, those under `/Game/ZERO/HeroAbilities/`, gets row `<Ability>_<Property>` (`CooldownDuration` → `_Cooldown`) keyed 0-3 with its current value and becomes Value 1 × that row; existing rows are linked, not overwritten); add/delete rows; a grid with levels 0-3 editable in place; and every ability value with its source (constant, row, another hero's table, or a missing row). The "hero folder" is the `HeroAbilities/<X>` folder most of the hero's abilities live in.
    - **Project Audit**.
  - Details panels edit the class defaults (CDO) directly: the edit marks the package dirty and calls `PostEditChangeBlueprintActors`. "Save..." opens the usual save-dirty dialog.
  - `ZL_HeroAudit` (`ZLHeroAudit::`) holds the data gathering and checks, mostly by property name through reflection, so protected members and BP variables work.
  - Hero checks:
    - Data asset: icon, description, display mesh/anim, `CharacterClass`.
    - Empty required slots (primary, secondary, ability 1/2, ultimate, reload); the same ability in two slots.
    - Mesh: no mesh or anim class; a Deadlock ABP without an anim set; melee montages or anim-set clips on a skeleton incompatible with the mesh (the Pocket-plays-Lash's-melee bug); empty anim-set clips (info); display mesh differs from the hero mesh (info).
    - Weapon: not a `UZL_GA_WeaponFire`; no `ProjectileClass`; a projectile that deals no damage.
    - No `DefaultGameplayEffect`; a `Current<X>` modifier starting above `Max<X>` (all four heroes: `CurrentAmmo` 50 vs `MaxAmmo` 30).
  - Ability checks:
    - Active slots: no icon, name or description; cooldown 0.
    - `CooldownDuration` without `CooldownTags`; charged ability with `TimeBetweenCharges` 0; the ultimate slot whose `Slot` isn't `UltimateSlot`.
    - Any editable bool with "Debug" in its name left on; `bDrawDebug` on its projectile.
    - An `FScalableFloat` pointing at a curve table row that doesn't exist (error: the value is 0).
  - The project audit covers the heroes the list shows (unlisted test characters only with that checkbox on). It also reports `/Game/ZERO/HeroAbilities/` Blueprints no hero uses (info), and shipping content (outside `/Game/Developers/`) with asset-registry dependencies on `Developers/rpm86/Deadlock` (error) or other Developers assets (warning).
  - "Export CSV" writes `Saved/ZeroLockAudit/{AbilityAudit,HeroSlots,Issues}.csv`. `AbilityAudit.csv` has one row per ability Blueprint: users, slot, cooldown, tags, charges, movement flag, net policy, icon, issues, and "Tunables" (the editable numbers, bools and enums declared below `UBaseGameplayAbility`).
  - Console versions (`ZL_WorkbenchCommands.cpp`, scriptable from the Cmd box or the MCP): `ZL.Workbench.CopyDefaultEffect <Hero|all>`, `ZL.Workbench.CurveTable <Hero> [link]` (heroes by display or short name; logs the rows). They mark assets dirty; save afterwards.
  - Driving the workbench through the MCP Slate inspector: Click selects list rows and segments, but an SButton needs Click then `PressKey Enter`, and after that click later mouse-ups can stop registering until the editor restarts. Use the console commands for scripted work.
  - The module can't include `Zero_BaseGameInstance.h` (it pulls in `OnlineSubsystem.h`), so the table is read by property name.
- `UEUW_AbilityPropertyEditor` (Editor Utility Widget): lists BP ability classes with a details view of their CDO filtered to the "Icon" category (hard-coded, `EUW_AbilityPropertyEditor.cpp:32`).
- `AEditorTestActor`: test actor dumping abilities on BeginPlay. Dead code.
- `UAnimGraphNode_ZLDeadlockHero`: anim graph node (Animation.md).

## Tools/
- `Tools/DeadlockIcons/deadlock_icons.bat [--all] [--download-only]`: downloads ability and item icons from `https://api.deadlock-api.com/v1/assets/items` (the old `assets.deadlock-api.com/v2/items` host is gone; same JSON as `Content/Scripts/response.json`) to `Saved/DeadlockIcons/{Images,ShopImages}`, then imports them with `ue_import_icons.py`. The bat runs it headlessly if no `UnrealEditor` process is running; otherwise it prints the `py "<path>/ue_import_icons.py"` line for the editor console. Each entry's `image` → `/Game/Data/Images/<Name>` (`TC_EditorIcon`), `shop_image` → `/Game/Data/ShopImages/<Name>_shop` (`TC_Default`); both no mips, never stream, sRGB. `<Name>` is the API `name` with Unreal's invalid object-name characters (whitespace, `'`, `’`, `/`, `.`, `,`, `(`, `)` …) replaced by `_`, which matches every icon already in the project (checked 2026-10-25: 585/585 and 153/153). The first entry with a PNG wins a shared name; SVG-only entries are skipped. Default = only names the project lacks; `--all` re-downloads every icon and replaces the textures in place (same asset path, so references keep working). Valve art: prototyping only, like the rest.
- `Tools/TestEOS_TwoPlayers.bat`: two `-game` instances on `L_CharacterMovementBasics` with `-CustomConfig=EOS`, logged in through the EOS Dev Auth Tool (`PORT` 8080, credentials `CRED1`/`CRED2` = `Dev1` / `Dev2`, set at the top of the bat; tool at `E:\Repo\EOS-SDK-...\SDK\Tools\EOS_DevAuthTool-win32-x64-*`). See GameFlow.md "EOS / online".
- `Tools/TestEOS_TwoPlayers_Packaged.bat`: same, for the packaged `Saved\Packaged\Windows\ZeroLock.exe` (per-instance logs `Host.log` / `Join.log`). Staging doesn't pack `Config/Custom/EOS`, so the bat copies that ini loose into the packaged `ZeroLock\Config\Custom\EOS\`, which then holds the EOS credentials: never ship that folder.
- `Tools/DeadlockImporter/`: Deadlock hero importer (README inside). `deadlock_import.bat <hero>` exports via Source2Viewer-CLI + headless Blender to `Saved/DeadlockImporter/<Hero>/`; `py ".../ue_import.py" <Hero>` in the editor console imports into `/Game/Developers/rpm86/Deadlock/<Hero>/`. Tool paths in `config.json` (Deadlock install, Source2Viewer-CLI, Blender). Needs Source2Viewer-CLI **≥ 20.1** (`E:/Tools/cli-windows-x64_20.1`): 20.0 fails on models recompiled after the Oct 2026 Deadlock update (`Unknown KVType ... 26`, e.g. Doorman), and the importer then only reports a missing `_cache/<Hero>/full/<Hero>.glb`.
