// ZeroLock telemetry API (Cloudflare Worker). Docs/Claude/Telemetry.md.
//
// How a Worker works: Cloudflare calls `fetch(request, env)` for every HTTP request that reaches the Worker's URL.
// `request` is the incoming HTTP request (method, URL, headers, body). We return a `Response` (status code, headers,
// body). `env` holds the "bindings" from wrangler.toml: env.DB (the D1 database), env.ASSETS (the dashboard files)
// and the secrets (env.INGEST_KEY, env.READ_KEY).
//
// Endpoints:
//   POST /v1/events            the game sends a batch of events            (needs INGEST_KEY)
//   GET  /v1/stats/<name>      the dashboard reads a summary for one chart (needs READ_KEY if it is set)
//   GET  /anything-else        the dashboard website (../dashboard)

// Limits, so a broken or hostile client can't fill the database.
const MAX_BODY_BYTES = 1_000_000;
const MAX_EVENTS_PER_REQUEST = 1000;
const MAX_STRING = 64;
const MAX_EXTRA = 1024;

// Event fields the game may send, and what type each must be. Everything else is ignored.
const STRING_FIELDS = ['match_id', 'type', 'player', 'hero', 'team', 'target', 'target_hero', 'target_team', 'ability', 'damage_type', 'item'];
const NUMBER_FIELDS = ['t', 'value', 'count', 'x', 'y', 'z'];

export default {
  async fetch(request, env) {
    const url = new URL(request.url);

    try {
      if (url.pathname === '/v1/events') {
        if (request.method !== 'POST') return json({ error: 'use POST' }, 405);
        return await handleIngest(request, env);
      }

      if (url.pathname.startsWith('/v1/stats/')) {
        if (request.method !== 'GET') return json({ error: 'use GET' }, 405);
        if (env.READ_KEY && bearer(request) !== env.READ_KEY) return json({ error: 'bad read key' }, 401);
        return await handleStats(url.pathname.slice('/v1/stats/'.length), url.searchParams, env);
      }

      if (url.pathname.startsWith('/v1/')) return json({ error: 'not found' }, 404);

      // Not an API call: serve the dashboard files.
      return env.ASSETS.fetch(request);
    } catch (err) {
      // Shows up in `wrangler dev` output / `wrangler tail`.
      console.error(err);
      return json({ error: 'server error' }, 500);
    }
  },
};

// ---------------------------------------------------------------------------------------------------------------------
// POST /v1/events
//
// Body sent by UZL_TelemetrySubsystem::SendBatch:
//   { "session": "a1b2...", "build": "0.1", "role": "host",
//     "events": [ { "match_id": "...", "t": 12.5, "type": "damage", "player": "...", "hero": "Pocket", "value": 120, ... }, ... ] }

async function handleIngest(request, env) {
  // 401 = "who are you?". The game sends the key in the Authorization header.
  if (!env.INGEST_KEY || bearer(request) !== env.INGEST_KEY) return json({ error: 'bad ingest key' }, 401);

  // 413 = "payload too large". Read the body as text first so we can check the size before parsing.
  const text = await request.text();
  if (text.length > MAX_BODY_BYTES) return json({ error: 'body too large' }, 413);

  // 400 = "bad request": not JSON, or not the shape we expect.
  let body;
  try {
    body = JSON.parse(text);
  } catch {
    return json({ error: 'body is not JSON' }, 400);
  }
  if (!body || !Array.isArray(body.events)) return json({ error: 'missing events array' }, 400);
  if (body.events.length > MAX_EVENTS_PER_REQUEST) return json({ error: 'too many events' }, 413);

  // Keep only known fields with the right types; drop events without a valid type.
  const rows = [];
  for (const event of body.events) {
    const row = cleanEvent(event);
    if (row) rows.push(row);
  }
  if (rows.length === 0) return json({ stored: 0 });

  // One SQL statement for the whole batch: the rows go in as one JSON array parameter, and SQLite's json_each()
  // turns it back into rows. (One INSERT per event would be hundreds of queries per request.)
  // The ?1 ?2 ... placeholders are filled by .bind(); never paste request data into SQL text (SQL injection).
  await env.DB.prepare(
    `INSERT INTO events (session, build, role, match_id, t, type, player, hero, team, target, target_hero, target_team,
                         ability, damage_type, item, value, count, x, y, z, extra)
     SELECT ?1, ?2, ?3,
            json_extract(value, '$.match_id'), json_extract(value, '$.t'), json_extract(value, '$.type'),
            json_extract(value, '$.player'), json_extract(value, '$.hero'), json_extract(value, '$.team'),
            json_extract(value, '$.target'), json_extract(value, '$.target_hero'), json_extract(value, '$.target_team'),
            json_extract(value, '$.ability'), json_extract(value, '$.damage_type'), json_extract(value, '$.item'),
            json_extract(value, '$.value'), json_extract(value, '$.count'),
            json_extract(value, '$.x'), json_extract(value, '$.y'), json_extract(value, '$.z'),
            json_extract(value, '$.extra')
     FROM json_each(?4)`
  )
    .bind(cleanString(body.session), cleanString(body.build), cleanString(body.role), JSON.stringify(rows))
    .run();

  // 200 OK with a small JSON answer. The game only looks at the status code.
  return json({ stored: rows.length });
}

function cleanEvent(event) {
  if (!event || typeof event !== 'object') return null;
  const type = event.type;
  if (typeof type !== 'string' || !/^[a-z_]{1,32}$/.test(type)) return null;

  // Builds before 2026-10-22 sent the victim as "target_player".
  if (event.target === undefined && event.target_player !== undefined) event.target = event.target_player;

  const row = {};
  for (const field of STRING_FIELDS) {
    const value = cleanString(event[field]);
    if (value !== null) row[field] = value;
  }
  for (const field of NUMBER_FIELDS) {
    const value = event[field];
    if (typeof value === 'number' && Number.isFinite(value)) row[field] = value;
  }
  if (event.extra && typeof event.extra === 'object') {
    const extra = JSON.stringify(event.extra);
    if (extra.length <= MAX_EXTRA) row.extra = extra;
  }
  return row;
}

function cleanString(value) {
  return typeof value === 'string' && value.length > 0 ? value.slice(0, MAX_STRING) : null;
}

// ---------------------------------------------------------------------------------------------------------------------
// GET /v1/stats/<name>?match=<match_id>
//
// Each name is one or more SQL queries; the dashboard draws the JSON it gets back. ?match= limits to one match.

async function handleStats(name, params, env) {
  const match = params.get('match') || null;
  // "AND (?1 IS NULL OR match_id = ?1)": with no ?match= every match counts.
  const inMatch = 'AND (?1 IS NULL OR match_id = ?1)';
  const all = (sql) => env.DB.prepare(sql).bind(match);

  switch (name) {
    case 'overview': {
      const row = await all(
        `SELECT
           (SELECT COUNT(*) FROM events WHERE type = 'match_start' ${inMatch}) AS matches,
           (SELECT COUNT(DISTINCT player) FROM events WHERE type = 'player_join' ${inMatch}) AS players,
           (SELECT COUNT(*) FROM events WHERE type = 'death' ${inMatch}) AS kills,
           (SELECT ROUND(SUM(value)) FROM events WHERE type = 'damage' ${inMatch}) AS damage,
           (SELECT COUNT(*) FROM events WHERE 1 = 1 ${inMatch}) AS events,
           (SELECT MAX(received_at) FROM events) AS last_event`
      ).first();
      return json(row);
    }

    case 'matches': {
      const { results } = await env.DB.prepare(
        `SELECT s.match_id, s.received_at AS started,
                json_extract(s.extra, '$.map') AS map, json_extract(s.extra, '$.mode') AS mode,
                e.value AS duration, json_extract(e.extra, '$.winner') AS winner,
                (SELECT COUNT(DISTINCT p.player) FROM events p WHERE p.match_id = s.match_id AND p.type = 'player_join') AS players
         FROM events s
         LEFT JOIN events e ON e.match_id = s.match_id AND e.type = 'match_end'
         WHERE s.type = 'match_start'
         ORDER BY s.id DESC LIMIT 50`
      ).all();
      return json(results);
    }

    case 'heroes': {
      // Several small queries in one round trip (batch), merged per hero below.
      const [picks, kills, deaths, damage] = await env.DB.batch([
        all(`WITH p AS (SELECT DISTINCT match_id, player, hero, team FROM events WHERE type = 'hero_pick' ${inMatch}),
                  e AS (SELECT match_id, json_extract(extra, '$.winner') AS winner FROM events WHERE type = 'match_end')
             SELECT p.hero, COUNT(*) AS picks,
                    SUM(CASE WHEN e.winner = p.team THEN 1 ELSE 0 END) AS wins,
                    COUNT(e.match_id) AS finished
             FROM p LEFT JOIN e ON e.match_id = p.match_id GROUP BY p.hero`),
        all(`SELECT hero, COUNT(*) AS n FROM events WHERE type = 'death' AND hero IS NOT NULL ${inMatch} GROUP BY hero`),
        all(`SELECT target_hero AS hero, COUNT(*) AS n FROM events WHERE type = 'death' ${inMatch} GROUP BY target_hero`),
        all(`SELECT hero, ROUND(SUM(value)) AS n FROM events WHERE type = 'damage' ${inMatch} GROUP BY hero`),
      ]);
      const heroes = {};
      const get = (hero) => (heroes[hero] ??= { hero, picks: 0, wins: 0, finished: 0, kills: 0, deaths: 0, damage: 0 });
      for (const r of picks.results) Object.assign(get(r.hero), { picks: r.picks, wins: r.wins, finished: r.finished });
      for (const r of kills.results) get(r.hero).kills = r.n;
      for (const r of deaths.results) get(r.hero).deaths = r.n;
      for (const r of damage.results) get(r.hero).damage = r.n;
      return json(Object.values(heroes).filter((h) => h.hero));
    }

    case 'abilities': {
      const [damage, uses] = await env.DB.batch([
        all(`SELECT hero, ability, damage_type, ROUND(SUM(value)) AS damage, SUM(count) AS hits,
                    SUM(COALESCE(json_extract(extra, '$.kills'), 0)) AS kills
             FROM events WHERE type = 'damage' ${inMatch}
             GROUP BY hero, ability, damage_type ORDER BY damage DESC LIMIT 60`),
        all(`SELECT hero, ability, SUM(count) AS uses FROM events WHERE type = 'ability_use' ${inMatch}
             GROUP BY hero, ability ORDER BY uses DESC LIMIT 100`),
      ]);
      return json({ damage: damage.results, uses: uses.results });
    }

    case 'kills': {
      // Death rows: player/hero = killer, target_hero = victim, ability/damage_type = the killing blow.
      // Older death rows have no damage_type: take it from a damage row of the same ability.
      const kills = `WITH k AS (
          SELECT d.hero, COALESCE(d.ability, 'unknown') AS ability, d.extra, d.count,
                 COALESCE(d.damage_type,
                          (SELECT x.damage_type FROM events x WHERE x.type = 'damage' AND x.ability = d.ability LIMIT 1),
                          'other') AS damage_type
          FROM events d WHERE d.type = 'death' AND d.hero IS NOT NULL AND (?1 IS NULL OR d.match_id = ?1))`;
      const [byAbility, byType, matchups] = await env.DB.batch([
        all(`${kills}
             SELECT hero, ability, damage_type, COUNT(*) AS kills,
                    ROUND(AVG(json_extract(extra, '$.distance'))) AS avg_distance, ROUND(AVG(count), 2) AS avg_assists
             FROM k GROUP BY hero, ability, damage_type ORDER BY kills DESC LIMIT 60`),
        all(`${kills} SELECT damage_type, COUNT(*) AS kills FROM k GROUP BY damage_type`),
        all(`SELECT hero AS killer, target_hero AS victim, COUNT(*) AS kills
             FROM events WHERE type = 'death' AND hero IS NOT NULL ${inMatch} GROUP BY hero, target_hero`),
      ]);
      return json({ byAbility: byAbility.results, byType: byType.results, matchups: matchups.results });
    }

    case 'damage_types': {
      const [dealt, taken] = await env.DB.batch([
        all(`SELECT hero, damage_type, ROUND(SUM(value)) AS damage, SUM(count) AS hits
             FROM events WHERE type = 'damage' ${inMatch} GROUP BY hero, damage_type`),
        all(`SELECT target_hero AS hero, damage_type, ROUND(SUM(value)) AS damage, SUM(count) AS hits
             FROM events WHERE type = 'damage' ${inMatch} GROUP BY target_hero, damage_type`),
      ]);
      return json({ dealt: dealt.results, taken: taken.results });
    }

    case 'timeline': {
      // Everything for one match over time: ?match=<id>, else the latest match.
      const id = match ?? (await env.DB.prepare(`SELECT match_id FROM events WHERE type = 'match_start' ORDER BY id DESC LIMIT 1`).first('match_id'));
      if (!id) return json({ match: null });
      const q = (sql) => env.DB.prepare(sql).bind(id);
      // Totals (damage/heal/uses) are flushed every ~10 s, so 10 s buckets line them up across machines.
      const bucket = 'CAST(t / 10 AS INTEGER) * 10';
      const [info, players, deaths, damage, heals, uses, items, samples] = await env.DB.batch([
        q(`SELECT s.received_at AS started, json_extract(s.extra, '$.map') AS map, json_extract(s.extra, '$.mode') AS mode,
                  e.value AS duration, json_extract(e.extra, '$.winner') AS winner,
                  json_extract(e.extra, '$.red_score') AS red_score, json_extract(e.extra, '$.blue_score') AS blue_score
           FROM events s LEFT JOIN events e ON e.match_id = s.match_id AND e.type = 'match_end'
           WHERE s.type = 'match_start' AND s.match_id = ?1 LIMIT 1`),
        // Each player's name (join) and last picked hero + team.
        q(`SELECT j.player, json_extract(j.extra, '$.name') AS name,
                  (SELECT p.hero FROM events p WHERE p.match_id = ?1 AND p.type = 'hero_pick' AND p.player = j.player ORDER BY p.id DESC LIMIT 1) AS hero,
                  COALESCE((SELECT p.team FROM events p WHERE p.match_id = ?1 AND p.type = 'hero_pick' AND p.player = j.player ORDER BY p.id DESC LIMIT 1), j.team) AS team
           FROM events j WHERE j.match_id = ?1 AND j.type = 'player_join' GROUP BY j.player`),
        q(`SELECT d.t, d.player, d.hero, d.team, d.target, d.target_hero, d.target_team, d.ability, d.count AS assists,
                  COALESCE(d.damage_type, (SELECT x.damage_type FROM events x WHERE x.type = 'damage' AND x.ability = d.ability LIMIT 1), 'other') AS damage_type,
                  json_extract(d.extra, '$.distance') AS distance
           FROM events d WHERE d.match_id = ?1 AND d.type = 'death' ORDER BY d.t, d.id`),
        q(`SELECT ${bucket} AS t, player, hero, team, damage_type, ROUND(SUM(value)) AS amount
           FROM events WHERE match_id = ?1 AND type = 'damage' GROUP BY 1, player, hero, team, damage_type ORDER BY 1`),
        q(`SELECT ${bucket} AS t, player, hero, team, ROUND(SUM(value)) AS amount
           FROM events WHERE match_id = ?1 AND type = 'heal' GROUP BY 1, player, hero, team ORDER BY 1`),
        q(`SELECT ${bucket} AS t, hero, ability, SUM(count) AS uses
           FROM events WHERE match_id = ?1 AND type = 'ability_use' GROUP BY 1, hero, ability ORDER BY 1`),
        q(`SELECT t, player, hero, team, item, value AS cost FROM events WHERE match_id = ?1 AND type = 'item_buy' ORDER BY t`),
        q(`SELECT t, player, hero, team, json_extract(extra, '$.hp') AS hp, json_extract(extra, '$.max_hp') AS max_hp,
                  json_extract(extra, '$.souls') AS souls
           FROM events WHERE match_id = ?1 AND type = 'pos' AND extra IS NOT NULL ORDER BY t`),
      ]);
      return json({
        match: { match_id: id, ...(info.results[0] ?? {}) },
        players: players.results, deaths: deaths.results, damage: damage.results, heals: heals.results,
        uses: uses.results, items: items.results, samples: samples.results,
      });
    }

    case 'players': {
      const [names, kills, deaths, damage] = await env.DB.batch([
        all(`SELECT player, json_extract(extra, '$.name') AS name, COUNT(DISTINCT match_id) AS matches
             FROM events WHERE type = 'player_join' ${inMatch} GROUP BY player`),
        all(`SELECT player, COUNT(*) AS n FROM events WHERE type = 'death' AND player IS NOT NULL ${inMatch} GROUP BY player`),
        all(`SELECT target AS player, COUNT(*) AS n FROM events WHERE type = 'death' ${inMatch} GROUP BY target`),
        all(`SELECT player, ROUND(SUM(value)) AS n FROM events WHERE type = 'damage' ${inMatch} GROUP BY player`),
      ]);
      const players = {};
      const get = (id) => (players[id] ??= { player: id, name: id, matches: 0, kills: 0, deaths: 0, damage: 0 });
      for (const r of names.results) Object.assign(get(r.player), { name: r.name || r.player, matches: r.matches });
      for (const r of kills.results) get(r.player).kills = r.n;
      for (const r of deaths.results) get(r.player).deaths = r.n;
      for (const r of damage.results) get(r.player).damage = r.n;
      return json(Object.values(players).filter((p) => p.player));
    }

    case 'items': {
      const { results } = await all(
        `SELECT item, COUNT(*) AS buys, ROUND(AVG(t)) AS avg_time, MAX(value) AS cost
         FROM events WHERE type = 'item_buy' ${inMatch} GROUP BY item ORDER BY buys DESC LIMIT 40`
      ).all();
      return json(results);
    }

    case 'perf': {
      const [summary, samples] = await env.DB.batch([
        all(`SELECT COALESCE(build, '?') AS build, role, ROUND(AVG(value), 1) AS fps,
                    ROUND(AVG(json_extract(extra, '$.frame_ms')), 1) AS frame_ms,
                    ROUND(AVG(json_extract(extra, '$.ping_ms'))) AS ping_ms,
                    SUM(json_extract(extra, '$.hitches')) AS hitches, COUNT(*) AS samples
             FROM events WHERE type = 'perf' ${inMatch} GROUP BY build, role`),
        // Time series only for one match: the chosen one, else the latest.
        all(`SELECT session, role, player, t, value AS fps, json_extract(extra, '$.ping_ms') AS ping_ms
             FROM events
             WHERE type = 'perf' AND match_id = COALESCE(?1, (SELECT match_id FROM events WHERE type = 'match_start' ORDER BY id DESC LIMIT 1))
             ORDER BY t LIMIT 5000`),
      ]);
      return json({ summary: summary.results, samples: samples.results });
    }

    case 'map': {
      const { results } = await all(
        `SELECT type, hero, team, target_hero, target_team, x, y
         FROM events WHERE type IN ('death', 'pos') ${inMatch} ORDER BY id DESC LIMIT 8000`
      ).all();
      return json(results);
    }

    default:
      return json({ error: `unknown stat '${name}'` }, 404);
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// Helpers

/** Reads "Authorization: Bearer <key>". */
function bearer(request) {
  const header = request.headers.get('Authorization') || '';
  return header.startsWith('Bearer ') ? header.slice(7) : '';
}

/** JSON response with a status code (200 = OK, 4xx = the caller's fault, 5xx = ours). */
function json(data, status = 200) {
  return new Response(JSON.stringify(data), {
    status,
    headers: { 'Content-Type': 'application/json', 'Cache-Control': 'no-store' },
  });
}
