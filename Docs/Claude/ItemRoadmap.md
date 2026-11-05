# Item system roadmap

To resume, tell Claude: **"continue the item roadmap"**, or name a phase ("start Phase 3"). Edit this file freely: change the order, cross things out, add notes. Tick items off as they're done.

Item data: 173 shop items (50 active, 37 with a key so far, 64 with procs, 9 imbue): 74 complete, 91 partial, 8 unsupported. `Saved/ItemImporter/report.md` lists what each item still needs. Rerun the import with `Tools/ItemImporter/item_import.bat --show-partial`: a plain run drops the shop back to the complete items.

## Done
- [x] **Phase 1, inventory:** sell refunds, upgrades with full credit, stats survive respawn, client inventory updates, shop icons follow the replicated inventory, shop closes with B.
- [x] **Phase 2, importer:** `Tools/ItemImporter`, 173 item assets, `DT_ZL_Items`, `UZL_GE_ItemStats`, Tier 5 (shown in the Tier 4 row). Shop shows 122 items: 8 complete, 114 partial.
- [x] **Soul counter:** `WBP_SoulCount` on the HUD and in the shop.
- [x] **Phase 3, missing stats:** health and out-of-combat regen, move and sprint speed, bullet speed, debuff and slow resist, slide, melee distance, air control, ability charges, duration and range. Not playtested yet.
- [x] **Phase 4, active items:** keys Z, X, V, G (C is Ability 2), four slots, HUD bar with radial cooldown, drag between keys with Tab held, generic self / ally / enemy / ground abilities, 19 actives. Not playtested yet.

- [x] **Phase 5, effect system:** `UZL_ItemProcComponent` runs `FZL_ItemProc` recipes from `PROC_RECIPES` in `build_items.py`, real damage amounts on hit events, a guard against procs triggering themselves, silence, disarm, stun, bullet immunity, unstoppable, stealth, cleanse. Not playtested yet.
- [x] **Phase 6, special cases:** headshot trigger, imbue picker and bonuses for one ability, dash, launch, resize, delayed casts, damage over time and areas on actives. User confirmed the imbue picker works (2026-11-05); the rest of the Phase 5 and 6 checklists may not have been run.

## Decide first
- [ ] Run the hero importer with `--stats-only`? At least `GE_Apollo_DefaultStats` still has 100 health, a 1 s fire interval and 30 ammo, so item numbers look far too strong. It changes hero balance, so it's your call.
- [ ] Confirm the Phase 2 checks not reported yet: upgrade Extended Clip → Huge Magazine for 800, Extra Health and Quick Fire stats, stats after respawn.

## Left over from Phases 3 and 4
Adding a stat: `EZL_ItemStat`, `UZL_GE_ItemStats`, then `STATS` in `build_items.py`. Adding an active: `ACTIVE_COMPLETE` or `ACTIVE_PARTIAL` in `build_items.py`. Adding a passive: a recipe in `PROC_RECIPES`, and `PROC_PARTIAL` if it leaves anything out.
- [ ] Stats outside the Phase 3 list (1-3 items each): `BONUS_ATTACK_RANGE_PERCENT`, `ALL_DAMAGE_MULTIPLIER`, `TECH_RESIST_REDUCTION`, `MOVEMENT_SPEED_SLOW_PERCENT`, `PARRY_COOLDOWN_REDUCTION_FIXED`, `AIR_MOVE_DISTANCE_INCREASE_PERCENT`, `BASE_HEALTH_PERCENT`, `HEALTH_MAX_PERCENT`, `ULTIMATE_COOLDOWN_REDUCTION_PERCENTAGE`, `ZOOM_INCREASE_PERCENT`. NPC stats wait for troopers.
- [ ] Ability duration and range only reach part of the kit: duration works on timed effects, not on the hero abilities that keep their own float (Pocket Affliction, Doorman's hotel...); range works on the cone and surface bases, not on hero-specific ranges and radii.
- [ ] 13 actives have no key yet, on purpose, because half an active misleads: `capacitor` (projectile), `return_fire`, `discord` (aura on an ally), `ability_refresher`, `ability_power_shard`, `restorative_locket`, `blood_tribute` (toggle), `split_shot`, `magic_carpet` (flight), `prism_blast`, `unstable_concoction`, `goose_egg`, and `non_player_bonus_sacrifice` (needs NPCs).
- [ ] The HUD status bar shows a debuff's full time, not the time shortened by debuff resist.
- [ ] A fifth active item can't be bought (no message yet: the purchase is just refused).

## Left over from Phase 5
Each partial recipe says what it lacks in `PROC_PARTIAL` or `ACTIVE_PARTIAL`.
- [ ] Build up: `BUILD_UP_PERCENT_SCALE` reads the source's 0.7 as 7% a shot (15 hits). A guess from the numbers: check it feels right. Build up also never fades, the source's `BuildUpDuration` isn't used.
- [ ] Triggers still missing: on kill or assist, on receiving a status (`upgrade_vex_barrier`, `upgrade_auto_cleanse`, `upgrade_cloak_of_opportunity`), on heal, on dash jump, while sliding, while airborne.
- [ ] Reducing a hit: `upgrade_spellbreaker`, `upgrade_deflecting_armor`, `upgrade_cheat_death`.
- [ ] Stacks that last: `upgrade_glass_cannon`, `upgrade_trophy_collector`, `upgrade_siphon_bullets`.
- [ ] Cooldown actions: `upgrade_runed_gauntlets`, `upgrade_absorbing_armor`, `upgrade_rechargingbullets`, `upgrade_ability_refresher`.
- [ ] Stealth: no reveal up close or on damage, and no see-through effect for the hero's own team.
- [ ] Dash reduction, which several slows also carry.
- [ ] No HUD status bar for item statuses yet (`SendStatusTimer`).

## Left over from Phase 6
- [ ] Item effects have no visuals or sounds: zones are invisible, a dash has no trail, a headshot looks like any hit.
- [ ] Resizing only scales the mesh: the capsule, and so the hitbox and the headshot height, stay full size.
- [ ] `ImbuedHit` misses damage from projectiles an ability fires. Those abilities need to pass their ability through as `DamageSource`.
- [ ] `upgrade_omnicharge_pendant`: giving a non-charged ability charges. Imbue bonuses only cover cooldown, range and duration.
- [ ] A picked ability can't be changed without selling the item.
- [ ] NPC items wait for troopers.

## Parked
- [ ] Minimap colours: on Red, allies are red on the team bar but blue on the minimap
- [ ] `WBP_ChargePhase` divide by zero, `WBP_HeroSwap` "Cannot add duplicate item into ListView"
- [ ] A hero swap empties the inventory with no refund
- [ ] Tier 5 row of its own in the shop
- [ ] Original item icons to replace the placeholders; review names in `names.json`
- [ ] Remove the old `Content/Data/Items/Generated` set and `DT_MasterItems`
- [ ] Valve art is tracked in the public repo (`Content/Developers`, `Content/Data/Images`, `ShopImages`) despite the rule in `CLAUDE.md`

## Rules for this work
- Item names, icons and description text never come from the source data: only stats, costs and mechanics.
- Hidden nodes: `read_graph_dsl` doesn't show the FlipFlop in `WBP_PlayerHUD`, so don't rewrite that graph.
- After MCP widget edits, restart the editor before testing PIE.
