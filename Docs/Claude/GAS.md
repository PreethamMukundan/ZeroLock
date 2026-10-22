# GAS: abilities, attributes, damage, items

Paths under `Source/ZeroLock/`. Hero-specific abilities: see Heroes.md.

## Architecture
- **ASC lives on the pawn.** `AZeroLockCharacter : AZeroMoverPawn, IAbilitySystemInterface` creates `UBaseCharAbilitySystemComponent "AbilitySystemComp"` (replicated, **Mixed**), `UBaseCharAttributeSet "AttributeSet"`, `UZero_Item_Inventory_Component "ItemInventory"` (`ZeroLockCharacter.cpp:46-52`). Owner = Avatar = pawn. `AZero_BasePlayerState` only holds TeamID/K/D/A.
- `InitAbilityActorInfo(this,this)` in `PossessedBy` (server), `OnRep_PlayerState` (client), `ResetCharacter`.
- Server init order (`PossessedBy`): InitAbilityActorInfo → `CreateVM_Att` → `InitializeAttributes()` (`:163`: applies BP `DefaultGameplayEffect` at L1 (per hero: `HeroAbilities/<Hero>/GE_<Hero>_DefaultStats`, Content.md); tag listeners `ZeroLock.Stun`→`Stunned`, `ZeroLock.Abilities.MovementLock`→`MovementLocked` (empty), `ZeroLock.Melee.Parry`→`Parry`; attribute delegates for health/speed/ammo) → `GiveAbilities()` (`:207`) → `InitializeFloatingStatusBar`.
- **Granting** (server, `GiveAbilities`): per-slot `TSubclassOf` properties on the character BP (`ZeroLockCharacter.h:206-233`), no hero data asset.
  - `DefaultAbilities[]` at L1 (passives, `UZL_StatusTimerAbility`, which every hero needs).
  - `PrimaryFireAbility` at L1.
  - `GrantAbilityOfClassX(Class, InputID, bBroadcast)` (`:240-273`) for `SecondryFireAbility`, `Ability_1`, `Ability_2`, `UltimateAbility`, `ReloadAbility`, `HeavyMeleeAbility`, `LightMeleeAbility`, `ParryAbility`: spec at **level 0** (= not yet upgraded; `LevelUpAbility` raises it), input tag `Zerolock.InputBindTags.*` in **DynamicSpecSourceTags** (UI slot lookup + cooldown matching), Passive TargetStyle → InputID None.
- **Input**: classic InputID enum `EGASAbilityInputID` (`ZeroLock.h:22-35`: None, Confirm, Cancel, Primary_Attack, Secondry_Attack, Ability_1, Ability_2, Ultimate, Melee, Parry, Reload). Enhanced Input handlers call `AbilityLocalInputPressed/Released`. Primary fire is a local timer (Weapons.md). `EGASTargetConfirmationStyle`: Instant, Quick, Confirm, Passive.
- **Prediction**: hero abilities mostly LocalPredicted; passives/self-buffs ServerOnly; status timers ServerInitiated. All damage/heal/soul changes server-only via ASC helpers. Custom client→server data: `ServerReportSlamLocation` (keyed by prediction key, `UZL_Task_WaitSlamLocation`), `TryConsumeClientReplicatedTargetData` (projectiles), replicated InputReleased events (`UZL_WaitChargeRelease_Task`). Victim movement = gameplay event to the victim's ASC → small ability → Mover `RequestSafe*` (Movement.md).

## Attribute set: `UBaseCharAttributeSet` (only one)
All replicated `COND_None` (health and charges `REPNOTIFY_Always`). Percent stats are whole numbers (20 = 20%).

| Group | Attributes |
|---|---|
| Meta (not replicated) | `Damage`, `Healing` |
| Health | `CurrentHealth`, `MaximumHealth`, `HealthRegeneration`, `HealingBonus` (source), `HealingReduction` (target) |
| Movement | `CurrentSpeed`, `CurrentJump` (**not wired to Mover**; handlers commented out / TODO) |
| Stamina | `Stamina` (server mirror of the Mover-simulated value; don't modify it with GEs), `MaxStamina` (default 3), `StaminaRegenRate` per second (default 0.2). Defaults set in the ctor; override in `DefaultGameplayEffect`. Air jumps and dashes cost 1 (Movement.md) |
| Economy | `Soul` |
| Weapon | `FireRate` (seconds between shots), `WeaponDamage` %, `FlatWeapon`, `WeaponResistance`, `WeaponResistanceReduction`, `WeaponLifeSteal`, `CurrentAmmo`, `MaxAmmo` |
| Spirit | `SpiritDamage` %, `FlatSpirit`, `SpiritResistance`, `SpiritResistanceReduction`, `SpiritLifeSteal` |
| Melee | `MeleeDamage`, `MeleeResistance`, `MeleeResistanceReduction`, `MeleeLifeSteal` |
| Misc | `UniversalDamage` %, `CooldownReduction` %, `DebufReduction`, `DurationExtension` (unread in GAS) |
| Charges | `AbilityCharges_1..4`, `MaxCharges_1..4` (4 = Ultimate) |

- `PreAttributeChange`: MaximumHealth change → `AdjustAttributeForMaxChange` (up: current += delta; down: clamp; first init: current = max).
- `PostGameplayEffectExecute` (`.cpp:57-193`): **Damage**: both sides `AZeroLockCharacter` with PlayerStates and **different TeamID** (friendly fire ignored) → read & zero `Damage` → `Damage.Tag.NonLethal` caps at 1 HP → `SourcePC->ShowDamageNumber` → subtract → `AddLastHit` (assists) → `HandleDeath()` at 0. **Healing**: clamp add, no team check. Clamps for health, regen, charges.
- `PostAttributeChange`: `FireRate` → `ChangeFireRate()`; `CurrentAmmo <= 0` → `Reload()`.
- Death: `HandleDeath` (once) → CancelAllAbilities, remove effects, DisableInput, `ServerHandleDeath` → GameFlow.md. Respawn: `ResetCharacter` (remove effects, re-init, re-activate passives, full health).

## Damage pipeline
1. Server caller: `SourceASC->ApplyWeaponDamage | ApplySpiritDamage | ApplyMeleeDamage(TargetASC, value)` (`BaseCharAbilitySystemComponent.cpp:59-135`): `IsUntouchable` → authority → spec of BP-set `GE_<Type>Class` (L1) with SetByCaller `Zerolock.DamageCalc.Weapon|Spirit|Melee` → apply → `Event.<Type>Hit` to self, `Event.<Type>Recieved` to target (+ client RPC to victim owner).
2. GE asset (BP) runs `UCalc_WeaponDamage` / `UCalc_Spirit_Damage` / `UCalc_Melee_Damage` (`GAS/Calculations/`). The calcs **capture the Source's `Damage` attribute**, so the GE must feed SetByCaller into that capture (verify in asset).
   - Weapon: `base = Damage + FlatWeapon`; `unmit = base × (1+WeaponDamage/100) × (1+UniversalDamage/100)`; `mit = unmit × (1 − (WeaponRes − WeaponResReduction)/100)`.
   - Spirit: same with spirit stats. Melee: `D + D×WeaponDamage/100×0.5 + D×MeleeDamage/100`, × universal, mitigation (see issues).
   - Adds dynamic tag `Damage.Tag.Weapon|Spirit|Melee` (damage-number styling); lifesteal heal applied inside the calc; outputs to target `Damage`.
   - No crits, headshots or falloff.
3. `PostGameplayEffectExecute` applies it (above).
- Healing: `ApplyHeal` → `GE_HealingClass`, SetByCaller `Zerolock.HealCalc.Healing` → `UCalc_Healing`: 0 if `Zerolock.Status.HealBlocked`; `heal × (1 + HealingBonus/100 − HealingReduction/100)`.
- Reload: `UCalc_Reload` sets CurrentAmmo = source MaxAmmo.

## ASC: `UBaseCharAbilitySystemComponent`
- Delegates `OnNewAbilityAdded(Spec)` (also rebroadcast for every spec on each `OnRep_ActivateAbilities`), `OnAbilityUpgraded`.
- `LevelUpAbility`, `ApplyGameplayEffect(Target, Class, Level)`, `ApplyGameplayEffectWithStacks`, `IsUntouchable` (`Zerolock.Untouchable`, sends `Event.UntouchableTrigger`).
- `SendGameplayEventToSelf` / `SendGameplayEventToTarget` (also client RPC to a remote victim's owner).
- `AdjustActiveEffectsDurationByPercentage` / `ByValue` (server): rescales active HasDuration GEs whose captured source tags include `ZerolockAbilities.Cooldown` or `Zerolock.InputBindTags` (cooldown reduction/refresh).
- Slam location plumbing, `TryConsumeClientReplicatedTargetData`, mutable active-effect accessors.

## Base ability classes (`GAS/`)
| Class | Use it for | Override |
|---|---|---|
| `UBaseGameplayAbility` | Root of all abilities. InstancedPerActor; `ActivationBlockedTags += ZeroLock.Abilities` (blocks on **any** `ZeroLock.Abilities.*` child incl. MovementLock/Blocked); cooldown GE `UZL_GE_BaseCooldown`; `CanActivateAbility` fails while the ASC has `ZeroLock.Stun` unless the ability has `AbilityTriggers` (event-triggered reactions like PullTo still run); UI props (`IconImage`, `AbilityName`, descriptions L1–3), `Slot`, `CooldownTags`, `CooldownDuration` (FScalableFloat), `StackTag`, charges (`bIsChargedAbility`, `MaxChargesConfig` 3, `TimeBetweenCharges` 0.5), `bIsMovementAbility` (can't activate while the ASC has `Zerolock.Status.MovementAbilitiesBlocked`; set in C++ on Lash Grapple/GroundStrike, Pocket FlyingCloak, Apollo FlawlessAdvance, Drifter Teleport; editable per BP). Helpers: `GetHeroAimDirection` (aim that matches on client and server; use it instead of the follow camera in anything the server traces), `ApplyGameplayEffectToTarget/ApplyGameplyEffectToSelf`, `ConeTraceMulti`, `ReverseConeTraceMulti`, `GetConeOverlap`, `SendVictimMoveEvent`, progress-bar and charge-phase UI helpers. | `ActivateAbility` (call `CommitAbility` yourself), `EndAbility` |
| `UZL_BasePlayAnimation_AndDo` | Commit → montage → anim-notify event (most hero abilities). Adds loose `ZeroLock.Abilities.MovementLock` while active. Needs `MontageToPlay`, `EventTagToWait`. `EndAbility` never replicates from a non-authority instance: the predicting client reaches the notify first, and its replicated end used to cancel the server instance before the server applied the damage/effects, so these abilities only worked when the host cast them. Every end path must therefore also run on the server (montage events, delay tasks, Mover tasks); real cancels still replicate through `CancelAbility`. | `OnAnimationPointTrigger` (default ends), `OnAnimationCancelled`, `OnAnimationCompleted` |
| `UBase_GA_TargetActors` | Target-actor reticle (`Targetclass` must derive `AGameplayAbilityTargetActor_Trace`); `TargetConfirmationStyle` Instant/Confirm/Quick. Its filter requires `AZeroLockCharacter`, so the engine `_Trace` actors (`LineTraceWithFilter`) can't hit world geometry; use `AZL_GATA_SurfaceTrace` for locations. Both the ability and `WaitTargetData` call `StartTargeting` (the reticle only spawns on the second, since `ReticleClass` is set in between). | `AbilityConfirmedAction`, `AbilityCancelledAction`, `OnTargetActorSpawned` (configure the actor before targeting starts), `CanConfirmTarget` (before the commit on client and server; false = free cancel) |
| `UZL_BaseProjectileThrowAbility` | Server-spawned `ARealProjectile`, waits `ProjectileHitTag` / `ProjectileMissTag`. | `OnEventRecived`, `MissOnEventRecived`, `FireProjectile` |
| `UBasePassive_GameplayAbility` | ServerOnly passive, auto-activates `OnAvatarSet`, waits `Event.WeaponHit`, applies `PassiveEffectToApply` to the victim. `UZL_Base_Passive_ApplyToSelf` applies to self. | `OnEventRecived` |
| `UZero_BaseSelfBuff` | ServerOnly: wait `TimeBeforeBuff` 0.1 s → apply `BuffOrDebufsToApply` to self. No commit. | |
| `UZero_BaseReloadAbility` | Wait `ReloadTime` 1 s → `ReloadEffect`. No commit. | |
| `UZeroBase_LightMelee` | Light melee: `IA_LightMelee` (`AZeroLockCharacter::EI_LightMelee`, defaults to the asset) → InputID `LightMelee`. Shares a key with heavy melee (`IA_HeavyMelee` has a Hold trigger): **tap = light, hold = heavy**. Light fires on release, only if `AZeroMoverPawn::GetHeavyMeleeTriggerCount()` didn't change since the press. LocalPredicted, commits. Plays `LightMeleeMontage` (ability, else hero, else `DeadlockAnimSet->LightMeleeMontage`); strikes once on `Event.Melee.Hit` (`TAG_EVENT_MELEE_HIT`, from a montage notify) or after `HitTime` 0.2 s, or at montage end: server `AZeroLockCharacter::PerformMeleeStrike` along the flat `GetHeroAimDirection()` (`StrikeDamage` 40, `StrikeRange` 120, `StrikeRadius` 50), same parry check as heavy. Only the server replicates its end. `LightMeleeAbility` defaults to `BP_LightMelee`. | |
| `UZeroBase_HeavyMelee` | Old montage + overlap version, hard-coded damage 90, no commit. **Not on any input**: heavy melee is the Mover lunge, whose strike is `AZeroLockCharacter::MeleeStrike` → `PerformMeleeStrike` (Movement.md). `ParryAbility` defaults to `BPGA_Parry` (a `Zero_BaseSelfBuff` applying `BPGE_Parry` → `ZeroLock.Melee.Parry`) and `ParriedStunEffect` to `BPGE_Stun` in the `AZeroLockCharacter` constructor; the Mover heroes never set ParryAbility, so their parry key did nothing. Native tags `TAG_STUN`, `TAG_MELEE_PARRY`. | |
| `UZL_GA_KnockbackAbility`, `UZL_GA_PullToAbility`, `UZL_GA_ReleaseAbility`, `UZL_GA_StopAbility` | Victim abilities triggered by `Zerolock.Movement.Forced/PullTo/Release/Stop` events → Mover `RequestSafe*`. | |
| `UZL_StatusTimerAbility` | Victim HUD status bar. `SendStatusTimer(Instigator, TargetASC, StatusTag, Duration)` (server) → `Zerolock.Event.StatusTimer`; `ClearStatusTimer` (same event, magnitude -1) ends that status's bar early. | |

Cooldown: `CommitAbility` → `ApplyCooldown` (spec of cooldown GE, `CooldownTags` into DynamicGrantedTags, SetByCaller `Ability.Cooldown.Duration` (native `TAG_SETBYCALLER_COOLDOWN_DURATION`; `UZL_GE_BaseCooldown` and `BP_ZL_GE` read their duration from it) = `GetCoolDownTime()` = `CooldownDuration[level] × (1 − CDR/100)`, or `TimeBetweenCharges` for charged, skipped at 0). Charges: server `OnGiveAbility` sets `MaxCharges_N`/`AbilityCharges_N` from `Slot` (each charged ability needs its own `Slot`); `CheckCost` needs ≥1 (client reads the replicated value, so it can predict an activation the server then rejects); server `ApplyCost` → `UBaseCharAbilitySystemComponent::ConsumeAbilityCharge`: −1, and if no recharge is running, applies `UZL_GE_BaseCooldown` for `GetCoolDownTime()` granting the slot's `Ability.Charges.Recharge.<Slot>` tag. When it expires (not premature removal) +1 and the next recharge starts until full, so charges come back one at a time. `ChargeRechargeDuration` was renamed `TimeBetweenCharges` (PropertyRedirect); `ChargeRechargeGEClass`/`RechargeDurationTag` and the BP recharge GEs in `Developers/rpm86/SlotsCharged` are no longer used.

## Target actors (`GAS/TargetActors/`)
- `AZL_GATA_SurfaceTrace : AGameplayAbilityTargetActor_Trace`: location targeting. Line trace from the player's camera against **WorldStatic only** (pawns never block), clipped to `MaxRange` (default 2000) around the hero. `SurfaceMode` (`EZL_SurfaceTargetMode`): `Ground` = normal within `MaxGroundAngle` (45°) of up, no drop to the floor when aiming at a wall; `Wall` = normal within `WallAngleTolerance` (20°) of horizontal; `Any` = all angles incl. ceilings. Output: `SingleTargetHit` (read with `GetHitResultFromTargetData`: `ImpactPoint` + `ImpactNormal`). Reticle gets `SetIsTargetValid` and is placed on the surface, Z aligned to the normal. **Placement preview**: the owning C++ ability can set `PlacementCheck` (`TFunction<bool(const FHitResult&, FTransform&)>`: turns the hit into the real placement transform, starting from hit point + X = normal, and returns whether it's allowed; ANDed into validity, runs every aim tick) and `PreviewBox` (placement-space box, cm): `PreviewComponent` shows `PreviewMesh` (engine Cube) scaled to the box at the placement, `PreviewMaterial` (engine `BasicShapeMaterial`, "Color" param) `ValidPreviewColor` green / `InvalidPreviewColor` red, only for the locally controlled `PrimaryPC`, only on surfaces `SurfaceMode` allows (Wall mode: hidden while aiming at the floor), no collision/shadow. Per ability: make a BP subclass of `AZL_GATA_SurfaceTrace` with its own `PreviewMesh`/`PreviewMaterial`/colors and set it as the ability's target actor class (Doorway: `TargetActorClass`); `PreviewBox` comes from the ability (Doorway: door bounds). `bDebug` draws the green/red sphere only when no preview is shown. Optional no-clip rules: `Clearance` (`FZL_PlacementClearance`, `GAS/TargetActors/ZL_PlacementClearance.h`, **off by default**, needs a valid `PreviewBox` as the footprint; client aim only, the ability must re-check on the server). The struct (placement space, X out of the surface, WorldStatic only): `bEnabled`; box overlap over the footprint extended to `FrontClearance`, starting `SurfaceInset` 2 off the surface and `FloorInset` 8 off the bottom; `bRequireFlushSurface` (3×3 traces must find the surface within `MaxSurfaceGap` behind, normal dot ≥ 0.9); `bRequireGround` (ground under both bottom sides within `MaxGroundStep`). `IsClear(World, Placement, Footprint, IgnoredActor)` is usable from any ability (Doorway's `DoorClearance`).
- Optional anchor: `MaxAnchorDistance` > 0 makes hits further than that from `AnchorLocation` invalid; hits on `InvalidActors` are invalid (Doorway's second door).
- Invalid confirm: `Confirm` style re-binds `GenericLocalConfirmCallbacks` (`LocalInputConfirm` clears them) and keeps aiming; `Instant`/`Quick` or `bCancelOnInvalidConfirm` cancel. Client-produced data: the server trusts the location.
- `AZL_GATA_ConeSingleTarget : AGameplayAbilityTargetActor_Trace`: one enemy hero in a cone (`MaxRange` default 1500 from `GetPawnViewLocation`, `ConeHalfAngle` 12 around the ability's `GetHeroAimDirection`; Hero object-type sphere overlap, alive, enemy team, WorldStatic LOS, plus the owning ability's `TargetCheck` lambda); the one closest to the aim line wins. Scans only for the local `PrimaryPC`. `IndicatorMesh` (engine Cylinder, `IndicatorRadius` 60 × `IndicatorThickness` 3, BasicShapeMaterial `IndicatorColor` green) at the target's feet, local only; reticle moved to the target if `ReticleClassX` is set. Output: `SingleTargetHit` on the target (`GetActors()[0]`). Static `IsValidConeTarget` for the server re-check. Invalid confirm behaves like the surface trace. Used by Hotel Guest.
- Test: `UZL_GA_Debug_SurfaceTarget` (`GAS/`, Confirm style, `SurfaceMode`/`MaxRange`/`bDrawLiveTrace` set via `OnTargetActorSpawned`): draws and logs the confirmed hit on client and server.

## Tasks (`GAS/Tasks/`)
`UGAST_PlayMontageAndWaitForEvent` (GASDocumentation port), `UZL_AbilityTask_MoverMoveTo` (velocity move, `bStopAtEnd`), `UZL_AbilityTask_Reach_MoverMoveTo` (ends on Walking/near target), `UZL_AbilityTask_MoverMoveToActor`, `UZL_Task_WaitSlamLocation` (camera trace, radius grows with height; client RPCs result), `UZL_WaitChargeRelease_Task` (hold-to-charge, sets Mover "Locked"→"Falling", perfect window), `UZL_WaitDelay_Task` (progress UI). **Legacy CharacterMovement tasks (no-ops on Mover pawns):** `UGAST_MeleeMoveTo`, `UZL_Task_ApplyRootMotion`.

## Tags
- Ini: `Config/DefaultGameplayTags.ini`, `Config/Tags/ZerolockDamageTags.ini`. Native: `GAS/ZL_GameplayTags.h/.cpp`, namespace `ZerolockGameplayTagsForBinding` (`TAG_INPUT_SECONDRY/ABILITY_1/ABILITY_2/ULTIMATE`, `TAG_UNTOUCHABLE`, `TAG_MOVEMENT_ROOTED` `ZeroLock.Movement.Rooted`, `TAG_ABILITIES_BLOCKED`, `TAG_SETBYCALLER_SPIRIT`, `TAG_DAMAGE_NONLETHAL`, `TAG_STATUS_AFFLICTED`, `TAG_STATUS_HEAL_BLOCKED`, `TAG_STATUS_HOTEL_GUEST` `Zerolock.Status.HotelGuest`, `TAG_STATUS_MOVEMENT_ABILITIES_BLOCKED` `Zerolock.Status.MovementAbilitiesBlocked` (outside `ZeroLock.Abilities` on purpose), `TAG_EVENT_STATUS_TIMER`). UI layer tags: UI.md.
- Main namespaces: `Event.*` (Weapon/Spirit/Melee/Heal Hit & Recieved, Kill, Death, Assist, UntouchableTrigger), `ZeroLock.Abilities` (+`.MovementLock`, `.Blocked`), `ZeroLock.Stun`, `ZeroLock.Melee.Parry`, `ZeroLock.Weapon.Reloading`, `Zerolock.Untouchable`, `Zerolock.DamageCalc.*`, `Zerolock.HealCalc.Healing`, `Zerolock.Item.Cost`, `Ability.Cooldown.Duration`, `ZerolockAbilities.Cooldown.<Ability>`, `Damage.Tag.*`, `Zerolock.Movement.*`, `Zerolock.Status.*`, `GameplayCue.<Hero>.*`, hero tags.
- Mixed `Zerolock`/`ZeroLock` casing (FName compare is case-insensitive). Most code uses `RequestGameplayTag("...", false)` strings, so typos fail silently.

## Items (shop)
- `UZero_Item_data` (`Items/`, PrimaryDataAsset): ItemID, Cost, SellRefundPercent (0.5), name/icon/description, `PassiveEffects` (GE), `GrantedAbilities`, `ItemType` (Weapon/Spirit/Vitality), `ItemTierType` (1–4), `NextItemsToUpgrade`. Assets `Content/Data/Items/Generated/<Type>/<Tier>/...` (tool-generated), table `DT_ItemList`.
- `UZL_GE_Item_PassiveEffect`: Infinite-duration base GE.
- `UZero_Item_Inventory_Component` (on character): replicated `Items` (MaxSlots 12), `ServerBuyItem(Item, UpgradedFrom)` (Soul check, sell old, add, deduct via SetByCaller `Zerolock.Item.Cost`, apply passive GE + grant abilities), `ServerSellItem`.

## New ability recipe
1. Pick a base: montage+notify → `UZL_BasePlayAnimation_AndDo`; reticle → `UBase_GA_TargetActors`; projectile → `UZL_GA_WeaponFire`-style predicted task (Weapons.md) or `UZL_BaseProjectileThrowAbility`; else `UBaseGameplayAbility`.
2. `Public/<Hero>/ZL_<Hero>_<Name>.h` + `Private/<Hero>/ZL_<Hero>_<Name>.cpp`. Tunables as `FScalableFloat` by level (spec level starts at 0).
3. Server-only effects: `Hero->GetMyAbilitySystemComp()->ApplySpiritDamage(Villan->GetMyAbilitySystemComp(), Damage)`; victim moves via `SendVictimMoveEvent`; self moves via Mover tasks / `RequestSafe*`; status bars via `SendStatusTimer`.
4. Add `ZerolockAbilities.Cooldown.<Name>` to `DefaultGameplayTags.ini`.
5. BP subclass in `Content/ZERO/HeroAbilities/<Hero>/<Name>/`: CooldownTags, CooldownDuration, Slot, name/icon/descriptions, montage/event tag, charges.
6. Assign on the hero BP slot (`Ability_1`, `Ability_2`, `SecondryFireAbility`, `UltimateAbility`); passives in `DefaultAbilities`.
7. Don't hold `ZeroLock.Abilities.*` tags unless you want to block every other ability.

## Known issues
- **Every ability BP "fails to compile" in 5.8**: `CooldownGameplayEffectClass '<GE>' grants no tags` (`UZL_GE_BaseCooldown`, `BP_ZL_GE`), because cooldown tags are added at runtime as DynamicGrantedTags. Compile Errors list is empty; the abilities still run. Fix: give the cooldown GE a static Grant Tags component (e.g. a parent cooldown tag).
- **`Damage` meta attribute isn't zeroed when the team check fails** (`BaseCharAttributeSet.cpp:104-114`) while calcs capture source `Damage`: friendly hits may pollute later outgoing damage. Verify against GE assets.
- Lifesteal runs inside executions before team/untouchable checks (heals on friendly hits; `ApplyHeal` fires every hit).
- Melee mitigation precedence bug `(1 − (W/100)×(1 − M/100))` (`Calc_Melee_Damage.cpp:119`). Resistances unclamped.
- Asset tag typo `"ZeroLLock.Abilities"` (`BaseGameplayAbility.cpp:24`) → `BlockAbilitiesWithTag` matches nothing.
- `ApplyGameplayEffectToTarget/Self` end the ability on null input but don't return (`:38-43`, `:64-69`).
- `SetSlot()` never called; charged abilities without BP `Slot` share `AbilityCharges_1`.
- A cooldown of 0 (`CooldownDuration` 0 at the ability's level, or 100% CDR) makes the cooldown GE instant: the server keeps nothing, while the client's predicted copy never expires and leaves the cooldown tag on.
- `GrantAbilityOfClassX` calls `SetInputID` on the CDO (`ZeroLockCharacter.cpp:270`).
- Victim Release/Stop abilities need TargetData but `SendVictimMoveEvent` only adds it with a location; DeathSlam sends them without (`ZL_Lash_DeathSlam.cpp:163,247`) → no-ops. Victim abilities are also blocked by the victim's `ZeroLock.Abilities.*` tags.
- `ApplyHeal`, `ApplyGameplayEffect*` lack authority checks; `IsUntouchable` runs before the authority check.
- `OnRep_ActivateAbilities` rebroadcasts every spec → duplicate UI risk.
- Persistent `DrawDebugSphere` in `UZL_Task_WaitSlamLocation` (`:96,114,156,196`); ZLOG spam in cooldown/duration code.
- `UZL_WaitDelay_Task` /0 on zero duration; `bStopAtEnd` freezes the pawn for the full move duration.
- `UZL_StatusTimerAbility` (ServerInitiated) calls UI helpers on the server.
- Items: **selling deducts souls instead of refunding** (`Zero_Item_Inventory_Component.cpp:82-84,178-181`); upgrade path broken (sells then `FindItem` returns null, `:42-54`); `ItemDelegate` server-only (no RepNotify); passive GEs applied at level 0.
- `UZeroBase_HeavyMelee`: damage hard-coded, hold code dead, unused. No commits on reload/selfbuff.
