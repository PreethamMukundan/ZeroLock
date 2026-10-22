-- One row per event. Columns every event can have; anything else goes into `extra` (JSON text).
-- Run: npm run db:init:local  (or db:init:remote for the live database)
CREATE TABLE IF NOT EXISTS events (
  id          INTEGER PRIMARY KEY AUTOINCREMENT,
  received_at TEXT NOT NULL DEFAULT (datetime('now')),
  session     TEXT,     -- one game launch on one machine
  build       TEXT,     -- ProjectVersion
  role        TEXT,     -- host / client / standalone
  match_id    TEXT,
  t           REAL,     -- seconds since match start
  type        TEXT NOT NULL,
  player      TEXT,     -- hashed player id (the one acting)
  hero        TEXT,
  team        TEXT,
  target      TEXT,     -- hashed player id of the one acted on
  target_hero TEXT,
  target_team TEXT,
  ability     TEXT,     -- ability / projectile class, or damage type when unknown
  damage_type TEXT,     -- weapon / spirit / melee
  item        TEXT,
  value       REAL,     -- damage, heal, fps, item cost, match length... depends on type
  count       INTEGER,  -- hits, uses, assists...
  x REAL, y REAL, z REAL,
  extra       TEXT
);

CREATE INDEX IF NOT EXISTS idx_events_type_match ON events(type, match_id);
CREATE INDEX IF NOT EXISTS idx_events_match ON events(match_id);
