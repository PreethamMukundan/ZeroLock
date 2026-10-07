# ZeroLock

Deadlock-inspired multiplayer hero shooter. Unreal Engine **5.8** (installed at `Z:\epicgames\UE_5.8`), C++ + Blueprints, Windows.
Main branch `main`; feature branches `feature/Hero/<Hero>`.

**Read the system doc in `Docs/Claude/` before searching the code.** Open source files only to edit them or to verify a detail.

| Doc | Covers |
|---|---|
| `Docs/Claude/Movement.md` | Mover pawn/component, modes, transitions, the `RequestSafe*` latch API, input, network prediction, controller/player state/camera |
| `Docs/Claude/GAS.md` | ASC setup, attribute set, damage/heal pipeline, base ability classes, tasks, tags, items/shop, new-ability recipe |
| `Docs/Claude/Heroes.md` | Every hero ability (Apollo, Lash, Pocket, Drifter, Xayah, Seven, Doorman), status, patterns to copy |
| `Docs/Claude/Weapons.md` | Primary fire, predicted projectiles (`AProjectile`, spawn task), prediction tuning |
| `Docs/Claude/GameFlow.md` | Game mode/state, teams, death/respawn, hero select, EOS lobbies/P2P |
| `Docs/Claude/UI.md` | CommonUI layers, MVVM viewmodels/resolvers, HUD, shop, menus |
| `Docs/Claude/Animation.md` | `ABP_DeadlockHero` C++ anim node/instance/anim set, Deadlock importer |
| `Docs/Claude/Content.md` | Content folder map, editor module, `Tools/` |

## Architecture in one screen
- **Hero** = `AZeroLockCharacter` → `AZeroMoverPawn` (APawn, **Mover plugin**, not ACharacter/CMC) with `UZeroMoverComponent`. Mover modes/transitions are configured in BP (`Content/ZERO/monkeyBoi/BP_Mover_Monkey`).
- **GAS on the pawn** (`UBaseCharAbilitySystemComponent`, Mixed replication, owner = avatar = pawn), single `UBaseCharAttributeSet`. Abilities granted from per-slot properties on the hero BP; InputID enum `EGASAbilityInputID` (`ZeroLock.h`).
- **Damage**: server-only via `ApplyWeaponDamage/ApplySpiritDamage/ApplyMeleeDamage` → BP GE → `UCalc_*` executions → `PostGameplayEffectExecute` (team check, death).
- **Weapons**: no weapon actor. Primary fire = GAS ability spawning client-predicted projectiles (`UZL_Task_SpawnPredictedProjectile`, `AProjectile`).
- **Movement from abilities**: Mover tasks / `RequestSafe*` latches (predicted through input). Move other players only with `SendVictimMoveEvent`.
- **UI**: CommonUI layer stacks (`UI.Layer.Game/Menu/Itemshop/PopUp`) + MVVM.
- **Online**: EOS lobbies, P2P (`P2PMODE=1`), listen server; login only in packaged builds.
- Modules: `ZeroLock` (runtime), `ZeroEditorModule` (editor). Plugins: Mover, ChaosMover, MoverExamples, GameplayAbilities, CommonUI, MVVM, EOS, ModelContextProtocol (Unreal MCP).

## Conventions
- Prefixes: `ZL_` for newer gameplay/UI classes (`UZL_<Hero>_<Ability>`, `UZL_VM_*`), `Zero_` for framework bases (`AZero_BasePlayerController`), `Zero<Name>Mode/Transition` for Mover classes. Code calls self `Hero` and enemies `Villan`.
- `Public/<System>/` ↔ `Private/<System>/`; includes are module-relative (`"Mover/ZeroMoverComponent.h"`).
- Header: `// Copyright Preetham Mukundan (C) 2026`.
- Logging: `ZLOG(x)` / `ZLOG_COLOR_TIME(x,c,t)` on-screen macros (`ZeroLock.h`, always compiled in) + `UE_LOG(LogTemp, ...)`.
- New gameplay tags: native in `GAS/ZL_GameplayTags.h/.cpp` (`ZerolockGameplayTagsForBinding::`); ini tags in `Config/DefaultGameplayTags.ini`, `Config/Tags/`. Avoid new `RequestGameplayTag("...", false)` strings (typos fail silently).
- Renaming a `UCLASS` that BPs use needs a CoreRedirect in `DefaultEngine.ini`.
- Pocket is the reference implementation for new hero abilities (see Heroes.md "Patterns to follow").
- Cosmetics inside Mover sim: guard with `!GetLastTimeStep().bIsResimulating` and not dedicated server.

## Rules
- **Valve/Deadlock assets** (`Content/Developers/rpm86/Deadlock/`, imported by `Tools/DeadlockImporter`) are for local prototyping only. Never ship them, never move them out of Developers, never reference them from shipping content, keep them out of shared branches.
- **Secrets**: `Config/DefaultEngine.ini` contains EOS client credentials. Never copy them into docs, commits messages, logs or chat.
- Don't commit or push unless asked.
- **Keep these docs current**: when you change a system, update its `Docs/Claude/*.md` (facts, tuning numbers, known issues) in the same change. Remove issues you fix.

## Build and editor
- Build editor target (editor must be **closed**, or Live Coding locks the DLL; Live Coding can't apply new UPROPERTYs/headers):
  `"Z:\epicgames\UE_5.8\Engine\Build\BatchFiles\Build.bat" ZeroLockEditor Win64 Development "-Project=E:\Repo\ZeroLock\ZeroLock\ZeroLock.uproject" -WaitMutex`
  A full rebuild takes ~15 min; incremental is much faster.
- Unreal MCP (`mcp__unreal-mcp__*`): use `list_toolsets` / `describe_toolset`, then `call_tool` with `toolset_name`. Useful: `ObjectTools.get_properties` on BP CDO subobjects (e.g. `/Game/ZERO/monkeyBoi/BP_Mover_Monkey.Default__BP_Mover_Monkey_C:MoverComponent`), `EditorAppToolset.OpenEditorForAsset` + `CaptureEditorImage` for screenshots. Some describe outputs are huge; grep the saved file.
- Editor console Python: `py "<path>.py" args`.

## Highest-impact known issues (details in the system docs)
- Security/trust: server trusts client projectile spawn transform and fire rate; `ServerHandleDeath` callable by any client; `Server_UpgradeAbility` unvalidated; latched Mover moves unvalidated (Weapons.md, GameFlow.md, UI.md, Movement.md).
- GAS: `Damage` meta attribute not reset on friendly hits; lifesteal before team check; melee mitigation precedence bug; `"ZeroLLock.Abilities"` typo (GAS.md).
- Items: selling deducts souls instead of refunding; upgrade path broken (GAS.md).
- Hero select never reaches the server (`ServerSetSelectedHero` uncalled), so everyone spawns `DefaultHeroClass` (GameFlow.md).
- Lash DeathSlam slam damage broken; Riposte can hang; Itani leaves client time dilation (Heroes.md).
- Mover: slide landing buffer unreachable, slide has no ground tag/ledge check; `GetSyncedAimRotation` always returns actor rotation; melee/zipline sync state not reset (Movement.md).
- `Config/DefaultEngine.ini:43` malformed `GameViewportClientClassName` line (UI.md, GameFlow.md).
