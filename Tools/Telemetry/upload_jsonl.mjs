// Uploads Saved/Telemetry/*.jsonl files (the game's local copy) to the telemetry API, e.g. after an offline playtest.
// Usage: node Tools/Telemetry/upload_jsonl.mjs <api-url> <ingest-key> <file.jsonl> [more files...]
// Uploading the same file twice stores its events twice.
import { readFileSync } from 'node:fs';
import { basename } from 'node:path';

const [url, key, ...files] = process.argv.slice(2);
if (!url || !key || files.length === 0) {
  console.error('usage: node upload_jsonl.mjs <api-url> <ingest-key> <file.jsonl>...');
  process.exit(1);
}

const BATCH = 500;
for (const file of files) {
  const events = readFileSync(file, 'utf8').split('\n').filter((l) => l.trim()).map((l) => JSON.parse(l));
  // File name is <date>_<session>.jsonl
  const session = basename(file, '.jsonl').split('_').pop();
  let stored = 0;
  for (let i = 0; i < events.length; i += BATCH) {
    const res = await fetch(new URL('/v1/events', url), {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', Authorization: `Bearer ${key}` },
      body: JSON.stringify({ session, build: 'upload', role: 'upload', events: events.slice(i, i + BATCH) }),
    });
    if (!res.ok) {
      console.error(`${file}: HTTP ${res.status} ${await res.text()}`);
      process.exit(1);
    }
    stored += (await res.json()).stored;
  }
  console.log(`${basename(file)}: ${stored}/${events.length} events stored`);
}
