# Content map, editor module, tools

## Content/
- `ZERO/`: **the game's own content.**
  - `Animation/Deadlock`: `ABP_DeadlockHero` (see Animation.md).
  - `BP/BaseGameplayAbility`: base ability/GE BPs (melee, parry, stun, reload, knockback, `GE_DamageBase`).
  - `BP/Character`: `BP_ZERO_BaseCharacter`. `BP/GameMode`: `BP_TDMGameMOde`, `BP_Zero_PlayerStart`. `BP/ITEMS`: `BP_Zero_Item_Data_Master`, `DT_ItemList`, `Vitality/`.
  - `Core`: `BP_ZeroGameInstance`, `ZeroBaseGameMode`, `BP_BaseZiplineActor`, `AN_TriggerEventNotify`.
  - `monkeyBoi/`: `BP_Mover_Monkey` (reference Mover pawn: modes + transitions configured here), abilities, montages. `Moveer/`: `BP_ZeromoverPawn`, Mover test game mode and movement settings.
  - `Bow_Bitch`, `GuNGuy`: older per-hero folders. `DeadlockChar/Pocket`: Pocket built from the Deadlock import.
  - `HeroAbilities/{ApolloTest,DrifterTest,LashTesst,PocketTest}/<Ability>/BP_<Hero>_<Ability>`: hero ability BPs.
  - `Levels`: `CharacterSelector.umap`, `Lvl_MainMenu.umap`. `MATS`: materials.
  - `UI/BP/...`: widget BPs (`Widgets/WBP_BaseUILayout`, `BP_BAseHUD`, `WBP_PlayerHUD`, `CharacterSelector/`, `DamageNumber/`, `Input/`, `OverHead/`, `Widgets/ItemShop`, `MainMenu`, `Abilities`, `StatBoxs`; `UI/BP/Weapon` holds fire-ability BPs, misplaced). `UI/CommonStyles`, `UI/CommonUI/Inputs`, `UI/Images`, `UI/Materials`.
- `Developers/rpm86/`: personal sandbox (test GEs, editor utility widgets `EUW_*`, `EOSTEST`, `GAS_TEST`).
  - **`Developers/rpm86/Deadlock/{Apollo,Lash,Pocket,_Shared}`: imported Valve Deadlock assets. Local prototyping only. Never ship, never reference from shipping content, keep out of shared branches.**
  - `BP_Deadlock_Lash` / `BP_Deadlock_Apollo`: Mover pawns using `ABP_DeadlockHero`.
- `Developers/Levi`: another dev's sandbox.
- Marketplace/template: `ParagonSparrow`, `ParagonSunWukong`, `ParagonTwinblast`, `Characters/Mannequin*`, `StarterContent`, `ThirdPerson`, `LevelPrototyping`, `UIMaterialLab`, `Blueprints` (template leftovers).
- `AbilityIcons/`, `Data/{Images,Items,ShopImages}`, `Scripts/response.json` (item API dump for the image downloader).
- `__ExternalActors__`, `__ExternalObjects__`: one-file-per-actor data.

## Editor module (`Source/ZeroEditorModule`)
- `FZeroEditorModuleModule`: adds a "ZeroLock" menu to the Level Editor with "BaseAbilityList" (logs every `UBaseGameplayAbility` subclass + properties). `CreatePropertyTool` is a no-op; the menu extender isn't removed on shutdown; `LogZeroEditor` declared but not defined.
- `UEUW_AbilityPropertyEditor` (Editor Utility Widget): lists BP ability classes with a details view of their CDO filtered to the "Icon" category (hard-coded, `EUW_AbilityPropertyEditor.cpp:32`).
- `AEditorTestActor`: test actor dumping abilities on BeginPlay. Dead code.
- `UAnimGraphNode_ZLDeadlockHero`: anim graph node (Animation.md).

## Tools/
- `Tools/DeadlockImporter/`: Deadlock hero importer (README inside). `deadlock_import.bat <hero>` exports via Source2Viewer-CLI + headless Blender to `Saved/DeadlockImporter/<Hero>/`; `py ".../ue_import.py" <Hero>` in the editor console imports into `/Game/Developers/rpm86/Deadlock/<Hero>/`. Tool paths in `config.json` (Deadlock install, Source2Viewer-CLI, Blender).
