# Heroes and their abilities

Code: `Source/ZeroLock/{Public,Private}/<Hero>/`. Ability BPs: `Content/ZERO/HeroAbilities/<Hero>Test/<Ability>/BP_<Hero>_<Ability>` (folders `ApolloTest`, `LashTesst`, `PocketTest`, `DrifterTest`). Xayah and Seven BPs live in `Content/Developers/rpm86/GAS_TEST/`. Base classes: GAS.md. Movement API: Movement.md.

## Status
| Hero | State |
|---|---|
| **Pocket** | 4/4 done, newest and cleanest code (predicted projectiles, careful EndAbility). Pending code review. Placeholder suitcase mesh; Barrage projectile debug draw on by default. |
| **Apollo** | 4/4 on Mover. Prediction rough edges, debug draws. |
| **Lash** | 4/4. DeathSlam slam damage broken (regression); GroundStrike victim drag broken for remote players. |
| **Drifter** | 4/4 on older patterns (legacy projectile, `SetActorLocation` teleport). WIP. |
| **Xayah** (LoL kit) | 5 abilities + projectile, test-only prototype in Developers. |
| **Seven** | Stun prototype only. **Doorman**: empty stubs. **MonkeyBoi**: `UZL_Ruyibang` stub. |

## Patterns to follow (Pocket is the reference)
- Constructor: `NetExecutionPolicy = LocalPredicted` (required for predicted projectiles); state via `ActivationOwnedTags` (`TAG_UNTOUCHABLE`, `TAG_MOVEMENT_ROOTED`, `TAG_ABILITIES_BLOCKED`).
- `ActivateAbility`: `if (!Hero || !CommitAbility(...)) { EndAbility(..., true, true); return; }`. Some abilities commit in `EndAbility` so the cooldown starts after a recast window (FlyingCloak, FlawlessAdvance, Riposte, DrifterTeleport).
- Tuning as `UPROPERTY(EditDefaultsOnly) FScalableFloat` read with `GetValueAtLevel(GetAbilityLevel())`.
- Movement: Mover tasks / `RequestSafeTeleport`; never `SetActorLocation` / `LaunchCharacter`. Prefer the Rooted tag over direct `QueueNextMode`. Move other players only with `SendVictimMoveEvent` (server `RequestSafe*` on a remote pawn is ignored).
- Timed windows: `UZL_WaitDelay_Task::WaitDealyWithProgressBar` (sic) + progress-bar UI functions; recast with `UAbilityTask_WaitInputPress`; jump-to-cancel with `Hero->OnJumpInputPressed` + `ConsumeJumpPress()`.
- Projectiles: subclass `AProjectile`, spawn with `UZL_Task_SpawnPredictedProjectile::SpawnPredictedProjectile(this, Class, ServerClass ? ServerClass : Class, Loc, Rot)`, gate damage with `HasAuthority() && !bIsFakeProjectile` (Weapons.md).
- Damage on server, filter `IsValid`, `!= Hero`, `!Hero->IsOnSameTeam(Villan)` (null-check first). DoTs: spec with SetByCaller `TAG_SETBYCALLER_SPIRIT` (see Affliction). Victim status UI: `UZL_StatusTimerAbility::SendStatusTimer`.
- `EndAbility`: idempotent cleanup guarded by a bool; unbind delegates before ending tasks; destroy spawned actors; queue the exit mode; `Super` last.
- New tags as natives in `ZL_GameplayTags.h/.cpp`.
- Target finding: newer code uses `OverlapMultiByObjectType(ECC_GameTraceChannel1)` ("Hero" object type); cone helpers from `UBaseGameplayAbility` don't filter teams.

## Apollo (`Apollo/`)
| Ability | Base | What it does |
|---|---|---|
| `UZL_Apollo_FlawlessAdvance` | `UBaseGameplayAbility` | Up to 3 lunges (`MaxLunges` 3, `RecastWindow` 5). Each: short move along input → `UZL_WaitChargeRelease_Task` (perfect window 0.6–0.9 → ×1.5 velocity, bonus damage + heal) → `ReverseConeTraceMulti` spirit damage → lunge (`LungeBurstVelocity` 2500, 0.15 s). Commits in `EndAbility`. |
| `UZL_Apollo_DisengagingSigil` | `UZL_BasePlayAnimation_AndDo` | Anim point: server capsule sweep 200 ahead for spirit damage, then hop back/sideways (`KnockbackStrength` 1000) over 0.1 s, knockback montage. |
| `UZL_Apollo_Riposte` | `UBaseGameplayAbility` | Parry stance (Locked, `ParryEffect` grants Untouchable, `MaxTime` 2). On `Event.UntouchableTrigger`: pick target with trace target actor → `MoverMoveToActor` (1 s) → stun sphere + resistance buff. Commits in `EndAbility`. |
| `UZL_Apollo_ItaniLoSahn` (ult) | `UBaseGameplayAbility` | 4 s charge (Locked) → release: shrink capsule, dash 0.25 s through enemies (sphere `DamageRadius` 500), victims get cue + `CustomTimeDilation` 0.05 → after 1.8 s server damage (+`BonusDamagePercent` under 50% HP). |

## Lash (`Lash/`)
| Ability | Base | What it does |
|---|---|---|
| `UZL_Lash_GroundStrike` | `UBaseGameplayAbility` | Slam toward aimed ground point (radius 600 + height×0.5, max 3000) at `MovementSpeed` 3000 via `Reach_MoverMoveTo`; drags enemies in a 250 sphere; server cone damage = base + distance×mult; L2+ KnockUp. |
| `UZL_Lash_DeathSlam` (ult) | `UBase_GA_TargetActors` | Hold: `AZL_GATargetActor_CylinderCharge` locks enemies in a camera cylinder (`LockOnThreshold` 1.5, r200 × 600; auto-confirm 2 s) → `SendVictimMoveEvent(PullTo)` to Lash → `UZL_Task_WaitSlamLocation` (high-ping fix: client reports slam point via `ServerReportSlamLocation`) → PullTo slam point → end. |
| `UZL_Lash_Flog` | `UZL_BasePlayAnimation_AndDo` | Whip cone (500, 45°) from camera: `FlogEffect` + spirit damage; heals damage × 0.6 per enemy. |
| `UZL_Lash_Grapple` | `UBaseGameplayAbility` | Camera cone; if a character is found, commit and launch toward it (`GrappleLaunchStrength` 1200 over 0.2 s) + self GE. No hit = no cost. |

## Pocket (`Pocket/`)
| Ability | Base | What it does |
|---|---|---|
| `UZL_Pocket_Barrage` | `UBaseGameplayAbility` | Float on momentum (`FloatingModeName` "Floating", a `UZeroFloatMode` with `bPreserveMomentum`) for `FloatDuration` 3 while firing `NumProjectiles` 4 predicted `AZL_Pocket_BarrageProjectile` (impact radius 200, spirit damage, +1 stack per enemy-hitting impact). Cancel by re-press, jump, or another ability (`OnAbilityFailed` cancels and activates the other). Remote server pre-creates spawn tasks and defers finish until they complete. |
| `UZL_Pocket_FlyingCloak` | `UBaseGameplayAbility` | Fires `AZL_Pocket_CloakProjectile` (slides along walls, never detonates, damages each enemy once). Re-press within `TeleportWindow` 2 → `RequestSafeTeleport` to it. Commits in `EndAbility`. |
| `UZL_Pocket_EnchantersSatchel` | `UBaseGameplayAbility` | Become a rooted, untouchable suitcase (`AZL_Pocket_SuitcaseActor`, owning client spawns its own copy) for up to `MaxDuration` 4; exit on re-press, jump, timeout or Cloak teleport; exit spirit damage in 400. Doesn't replicate EndAbility when ended by a teleport on a remote client (RPC could beat movement input). |
| `UZL_Pocket_Affliction` (ult) | `UZL_BasePlayAnimation_AndDo` | All enemies in `Radius` 800 get `UZL_Pocket_AfflictionEffect` (C++ GE: periodic `UCalc_Spirit_Damage`, 6 s, 1 s ticks, 10/tick, NonLethal, `Zerolock.Status.Afflicted`, HealBlocked from level 3) + status timer. |

## Drifter (`Drifter/`)
| Ability | Base | What it does |
|---|---|---|
| `UZL_Drifter_BloodScent` (passive) | `UBasePassive_GameplayAbility` | 2000 detection sphere; every 0.2 s marks enemies with no teammate within 700; self buff while any is isolated; kill on marked → 4 stacks, L2+ cooldown refresh 15 s. |
| `UZL_Drifter_Rend` | `UZL_BasePlayAnimation_AndDo` | Cone melee damage 25 + spirit bonus within 50 (set `ConeHeight` in BP, C++ default 0). |
| `UZL_Drifter_EternalNight` (ult) | `UZL_BasePlayAnimation_AndDo` | Self buff + blind up to 3 enemies in 1000; for 5 s weapon hits deal +15 spirit. |
| `UZL_GA_Drifter_Teleport` (Stalker's Mark) | `UZL_BaseProjectileThrowAbility` | Legacy projectile marks a target; re-press within the mark duration teleports behind it with `SetActorLocation` (bypasses Mover). |

## Xayah (`Xayah/`, prototype)
`UZL_Xayah_CleanCut` (passive, tracks feathers), `UZL_Xayah_Double_daggers` (2 projectiles), `UZL_Xayah_DeadlyPlumage` (buff + feathers), `UZL_Xayah_Bladecaller` (recall feathers), `UZL_Xayah_Featherstorm` (10-feather fan). `AZL_Xayah_projectile : AZero_BaseProjectile` (legacy, not predicted, hard-coded damage 1 / 5, no team check).

## Seven / Doorman
`UZL_GA_Seven_Stun` (`UBase_GA_TargetActors`: start effect, then stun after `StunDelay`). `UZL_Doorman_CallBell` + `AZL_Projectile_Doorman_CallBell`: empty.

## Known issues
- **DeathSlam never deals slam damage / AfterSlamEffect**: `CurrentActiveEffectHandles` is never filled since commit `6155d0d` (`ZL_Lash_DeathSlam.cpp:215-234` dead); pull only happens if `LockOnEffect` is set (`:132`). Stop/Release events have no TargetData, so they no-op (GAS.md).
- **Riposte can hang**: dash timeout (`OnTimeExpired`) has no listener (`ZL_Apollo_Riposte.cpp:182-213`) → ability never ends, MovementLock blocks all abilities.
- FlawlessAdvance recast timer not cancelled on recast (`:242-258`) → can end mid-charge.
- ItaniLoSahn: client never resets `CustomTimeDilation` (`:160`, `:241`); not restored on cancel; client target data sent but never consumed.
- GroundStrike: drag uses server `RequestSafeAbilityMove` (ignored for remote players) with a displacement as velocity (`ZL_Lash_GroundStrike.cpp:171-178`); use `SendVictimMoveEvent(PullTo)`. `IsOnSameTeam` before null check (`:141`).
- Missing `return` after EndAbility on invalid Hero: Flog `:18-21`, Grapple `:22-25`, Rend `:23-26`, EternalNight `:20-23`, Featherstorm `:30-33`, Drifter Teleport `:27-30`.
- BloodScent: `Event.Assist` bound to the kill handler (`:48`); null deref `:174`.
- `!X.ActivationMode == Authority` precedence (`ZL_Drifter_BloodScent.cpp:64`, `ZL_Xayah_Double_daggers.cpp:62`).
- FlyingCloak passes `ServerProjectileClass` without fallback (`:42`).
- No team filtering in cone traces (FlawlessAdvance, Rend, Grapple).
- Direct `QueueNextMode` in Riposte, Itani, DeathSlam, Barrage → desync risk. Itani capsule/camera changes are per-machine.
- Duplication: `GetLookAtLocation` ×3, projectile spawn-transform code ×4, Pocket sphere-enemy helper ×2. Double commits in Flog/Rend.
- Debug draws and visible debug spheres left on in most heroes; `ZLOG` everywhere.
