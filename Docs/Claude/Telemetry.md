# Telemetry: in-game stats → Cloudflare API → dashboard

Free stack: the game POSTs JSON batches to a Cloudflare Worker (`Tools/Telemetry/worker`), which stores them in a D1 (SQLite) database and serves the dashboard (`Tools/Telemetry/dashboard/index.html`, Chart.js) from the same URL. Dashboard tabs (URL hash): Matches (KPIs, match list: click opens that match's timeline and filters every tab, match length, players), Match timeline (one match, the selected or latest: running kills per team with killer/victim/ability/type/distance tooltips, kill feed, damage per 10 s by team with kill markers, cumulative damage per player, damage per 10 s by type, healing per 10 s, souls with item-buy markers, health %, ability uses per 10 s without victim-side helper abilities), Heroes (pick rate, win rate, damage by hero split by type, damage per pick), Damage types (share, damage taken per hero, per ability), Kills (by killing ability, by type, killer×victim), Items, Performance, Map. The header match selector filters every tab.

```
UZL_TelemetrySubsystem ──POST /v1/events (JSON, Bearer INGEST_KEY)──► Worker ──► D1 table `events`
  + Saved/Telemetry/*.jsonl                                          Worker ◄── GET /v1/stats/<name> ── dashboard (same origin)
```

## Game side: `UZL_TelemetrySubsystem` (`Telemetry/ZL_TelemetrySubsystem.h`, GameInstanceSubsystem)
- **Server (host) records gameplay**, every instance records its own perf. A match = one game mode lifetime: `AZero_BaseGameModeBase::BeginPlay` → `BeginMatch` (GUID, copied to replicated `AZero_BaseGameState::TelemetryMatchId` so clients tag perf samples), `EndPlay` → `EndMatch(red, blue score)` (no win condition exists yet, so the winner is the higher team score when the map ends). Game modes not derived from `AZero_BaseGameModeBase` (e.g. ThirdPersonMap's `MoverTestGamemode`) start no match, so nothing is recorded there (perf samples need a match id too).
- Core ticker (`Tick`): accumulates frame times; every `PerfSampleInterval` (10 s) a `perf` event; every `PositionSampleInterval` (5 s, server) a `pos` event per living hero (`extra`: `hp`, `max_hp`, `souls`, for the timeline); every `FlushInterval` (10 s) `Flush`.
- **Totals**: damage, heal and ability uses are summed in memory per combination (attacker, victim, source) and emitted as one `damage` / `heal` / `ability_use` row per flush (`value` = sum, `count` = hits/uses, `extra.kills` = killing blows). Everything else is one row per event.
- `Flush` → appends JSON lines to `Saved/Telemetry/<date>_<session>.jsonl` (`bWriteLocalFile`) and, if `IngestUrl` is set, `SendBatch` per `MaxEventsPerRequest` (500): `FHttpModule` POST `<IngestUrl>/v1/events`, body `{session, build, role, events:[...]}`. Response callback on the game thread: 2xx ok; no connection / 5xx / 429 → events go back to the buffer for the next flush; other codes (401 bad key, 400) dropped. Failed batches wait in `RetryEvents` (resent next flush, not rewritten to the local file). One warning per failure streak (`LogZLTelemetry`). Buffer capped at `MaxBufferedEvents` (oldest dropped).
- Hooks:
  | Event | Where |
  |---|---|
  | `match_start` / `match_end` | `AZero_BaseGameModeBase::BeginPlay` / `EndPlay` |
  | `player_join` (extra.name) / `player_leave` | GM `BeginPlay` (already logged in) + `PostLogin` / `Logout`. Seamless-travel arrivals don't call PostLogin (same gap as team assignment, GameFlow.md) |
  | `hero_pick` (extra.initial) | GM `BeginPlay` (pawns spawned before it), `SpawnDefaultPawnFor`, `SwapHero` |
  | `damage` | `UBaseCharAttributeSet::RecordDamageTelemetry` after health is reduced; `value` = health actually lost (no overkill) |
  | `heal` | attribute set Healing branch (effective heal) |
  | `ability_use` | `UBaseCharAbilitySystemComponent::NotifyAbilityActivated` (authority; primary fire counts every shot) |
  | `death` | `HandlePlayerDeath`: `player` = killer, `target` = victim, `ability` + `damage_type` = the killing blow (last damage source on the victim), `count` = assists, x/y/z = victim, `extra` killer position + distance |
  | `item_buy` / `item_sell` | `UZero_Item_Inventory_Component::ServerBuyItem` / `ServerSellItem` |
  | `perf` | `value` = avg FPS, extra `frame_ms`, `frame_ms_max`, `hitches` (>50 ms), `ping_ms` |
  | custom | `RecordCustomEvent(Type, Map<string,string>)` (BlueprintCallable) |
- **Damage source**: `ApplyWeaponDamage/ApplySpiritDamage/ApplyMeleeDamage(Target, Value, DamageSource = nullptr)`. Every C++ call site passes `this` (the ability or projectile); the helper puts `CleanClassName(DamageSource)` in `ASC->TelemetryDamageSource` for the duration of the apply (`TGuardValue`), and the attribute set reads it from the source ASC. Fallbacks: the GE context's ability, else the damage type. `damage_type` comes from the `Damage.Tag.Weapon|Spirit|Melee` dynamic tag the calcs add. `AZeroLockCharacter::PerformMeleeStrike(..., TelemetrySource)` sets the name itself (`LightMelee` from `UZeroBase_LightMelee`, `HeavyMelee` from the lunge); a helper called without `DamageSource` keeps a name the caller set. BP callers pass nothing → type name. Weapon hits show the projectile class (e.g. `ProjectileTest`).
- The victim is `target` (`AddHero` with prefix `target_` writes `target`, `target_hero`, `target_team`). Builds before 2026-10-22 sent `target_player`; the API maps it to `target`.
- Names: `CleanClassName` strips `_C` and any leading `BP_`, `ZL_`, `GA_`, `Deadlock_`, `_` (so `ZL_Pocket_X` and `BP_Pocket_X` both read `Pocket_X`). Player ids: first 12 hex of MD5(unique net id), else `p<PlayerId>`; the readable name is only in `player_join.extra.name`.

### Config
- `Config/DefaultGame.ini` `[/Script/ZeroLock.ZL_TelemetrySubsystem]`: `bEnabled`, `bWriteLocalFile`, `FlushInterval`, `PerfSampleInterval`, `PositionSampleInterval` (0 = off), `MaxEventsPerRequest`, `MaxBufferedEvents`.
- **URL + key are not in the repo**: `Config/Telemetry.ini` (gitignored), section `[Telemetry]`, keys `IngestUrl`, `IngestKey`, `bEnabled`. **Quote the values** (`IngestUrl="http://127.0.0.1:8787"`): unquoted ini values are cut at `//`, leaving `http:`; `LoadLocalOverrides` then logs an error and falls back to local file only. Also read from `<ProjectDir>/Telemetry.ini` (packaged builds: next to the packaged project folder, copied by hand, like the EOS config). Command line overrides: `-TelemetryUrl=`, `-TelemetryKey=`, `-NoTelemetry`. No `IngestUrl` = local file only.
- The ingest key ships inside the game, so treat it as public: it can only add events (validated, size-capped). The read key protects the dashboard.

## API: `Tools/Telemetry/worker` (Cloudflare Worker, JavaScript)
- `wrangler.toml`: Worker `zerolock-telemetry`, D1 binding `DB` (`database_id` placeholder until `d1 create`), static assets `../dashboard` (binding `ASSETS`, `/v1/*` runs the Worker first). Secrets: `INGEST_KEY` (required to ingest), `READ_KEY` (optional; if set, `/v1/stats/*` needs `Authorization: Bearer <READ_KEY>`; the dashboard prompts once and keeps it in localStorage). Local values in `.dev.vars` (gitignored: `INGEST_KEY=dev-ingest-key`).
- `POST /v1/events`: 401 bad key, 413 body > 1 MB or > 1000 events, 400 not JSON / no `events` array. Each event keeps only known string fields (≤ 64 chars), finite numbers and `extra` (≤ 1 KB JSON); `type` must match `^[a-z_]{1,32}$` or the event is dropped. One INSERT per request via `json_each(?4)` over the cleaned array.
- `GET /v1/stats/<name>[?match=<id>]`: `overview`, `matches`, `heroes` (picks, wins vs `match_end.extra.winner` by team, kills, deaths, damage), `abilities` (damage by hero/ability/type, uses), `kills` (by killing ability + damage type, by type, killer×victim hero; death rows without `damage_type` take it from a damage row of the same ability), `damage_types` (dealt and taken per hero × type), `timeline` (one match: info, players with name/last hero/team, deaths, damage/heal/ability uses in 10 s buckets, item buys, hp/souls samples; latest match without `?match=`), `players`, `items`, `perf` (summary by build/role + FPS series of the chosen or latest match), `map` (death + position x/y, latest 8000).
- `schema.sql`: single `events` table (columns = the event fields + `session`, `build`, `role`, `received_at`), indexes on `(type, match_id)` and `match_id`.
- Free-tier limits that matter: 100k Worker requests/day, D1 100k rows written/day, 5 GB. A 12-player match writes roughly 150 position rows/min + a few dozen totals per flush; raise `PositionSampleInterval` if that gets close.

## Running it
Local (no account): `cd Tools/Telemetry/worker`, `npm install`, `npm run db:init:local`, `npm run dev` → API + dashboard on http://127.0.0.1:8787. `Config/Telemetry.ini` with `IngestUrl=http://127.0.0.1:8787`, `IngestKey=dev-ingest-key` points PIE at it. Local data lives in `worker/.wrangler/` (gitignored).

**Deployed 2026-10-22** (Worker `zerolock-telemetry`, D1 `zerolock-telemetry` region WNAM, id in `wrangler.toml`; secrets set). The live URL and keys are only in `Config/Telemetry.ini` (gitignored): `IngestUrl`, `IngestKey`, and the dashboard password as the comment `DashboardReadKey=`. Redeploy after changing `worker/` or `dashboard/`: `cd Tools/Telemetry/worker && npm run deploy` (data is untouched). Rotate a key: `npx wrangler secret put INGEST_KEY` (then update `Telemetry.ini` and every build that ships it). Backup: `npx wrangler d1 export zerolock-telemetry --remote --output=backup.sql`. A new workers.dev subdomain needs a few minutes for its TLS certificate (TLS handshake failures until then).

Deploy from scratch (free Cloudflare account):
1. `npx wrangler login`
2. `npx wrangler d1 create zerolock-telemetry` → paste the `database_id` into `wrangler.toml`
3. `npm run db:init:remote`
4. `npx wrangler secret put INGEST_KEY` (new random value) and optionally `npx wrangler secret put READ_KEY`
5. `npm run deploy` → `https://zerolock-telemetry.<subdomain>.workers.dev`
6. Put that URL and the ingest key in `Config/Telemetry.ini`.

Upload local files (offline playtest, API was down): `node Tools/Telemetry/upload_jsonl.mjs <api-url> <ingest-key> Saved/Telemetry/<file>.jsonl...` (uploading a file twice duplicates it).

Ad-hoc SQL: `npx wrangler d1 execute zerolock-telemetry --remote --command "SELECT type, COUNT(*) FROM events GROUP BY type"` (`--local` for dev). Live logs: `npx wrangler tail`.

## Known issues
- No rate limiting beyond the per-request caps; a leaked ingest key lets anyone add junk rows (rotate with `wrangler secret put INGEST_KEY`).
- Clients don't know the match start time, so their `t` is `AZero_BaseGameState::myGameTime` (whole seconds).
- Requests still in flight when the game closes can be lost (the local `.jsonl` keeps them).
- Abilities that deal damage through BP calls without `DamageSource`, or through GEs applied outside the helpers, are labelled by the GE's ability or the damage type only.
