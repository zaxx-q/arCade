// arCade online leaderboard: a Cloudflare Worker with a D1 database. Wire format: core/include/arcade/lbnet.h.
//
//   POST /v1/register                      -> "ok|<token>"                 (once per install)
//   POST /v1/scores   "table|player|score" -> "ok|<rank>|<players>"        (needs the token)
//   GET  /v1/top?board=<table>&limit=<n>   -> "ok|<players>" + one "rank|player|score|stamp" per row (public)
//
// The server keeps the best score per (table, install). Everything the client says is checked (protocol.ts),
// and the status codes mean what the client expects: 401 = unknown token (it registers again), 429/5xx =
// try later, any other 4xx = refused for good (the client drops that score), so a bug here must never answer
// a good request with a 4xx.
import type { Env, RateLimiter } from "./env";
import {
  MAX_BODY,
  bearerToken,
  errBody,
  hashToken,
  newToken,
  parseLimit,
  parseSubmit,
  parseTable,
  registerOk,
  submitOk,
  topOk,
  type TopRow,
} from "./protocol";

const TEXT_HEADERS = {
  "content-type": "text/plain; charset=utf-8",
  "cache-control": "no-store",
  "x-content-type-options": "nosniff",
};

function reply(status: number, body: string, extra: Record<string, string> = {}): Response {
  return new Response(body, { status, headers: { ...TEXT_HEADERS, ...extra } });
}

const fail = (status: number, reason: string, extra?: Record<string, string>): Response =>
  reply(status, errBody(reason), extra);
const tooMany = (): Response => fail(429, "rate_limited", { "retry-after": "30" });
const notAllowed = (allow: string): Response => fail(405, "method_not_allowed", { allow });

async function limited(limiter: RateLimiter, key: string): Promise<boolean> {
  return !(await limiter.limit({ key })).success;
}

/** The request body as text, or null when it is bigger than `max` bytes (it is never read past that). */
async function readBody(request: Request, max: number): Promise<string | null> {
  if (Number(request.headers.get("content-length") ?? "0") > max) return null;
  if (request.body === null) return "";
  const reader = request.body.getReader();
  const chunks: Uint8Array[] = [];
  let size = 0;
  for (;;) {
    const { done, value } = await reader.read();
    if (done) break;
    size += value.byteLength;
    if (size > max) {
      await reader.cancel();
      return null;
    }
    chunks.push(value);
  }
  const all = new Uint8Array(size);
  let at = 0;
  for (const chunk of chunks) {
    all.set(chunk, at);
    at += chunk.byteLength;
  }
  return new TextDecoder().decode(all);
}

// A worse run changes nothing (the WHERE), which also keeps D1 writes down. Equal scores keep the earlier run.
const UPSERT_SQL = `
  INSERT INTO scores (board, install, player, score, updated_at) VALUES (?1, ?2, ?3, ?4, ?5)
  ON CONFLICT (board, install) DO UPDATE SET
    player = excluded.player, score = excluded.score, updated_at = excluded.updated_at
  WHERE excluded.score > scores.score`;

// Place of one install in its table, in the order of the scores_rank index (and of the C client's outranks()).
const PLACE_SQL = `
  SELECT
    1 + (SELECT COUNT(*) FROM scores o WHERE o.board = me.board AND (
           o.score > me.score
           OR (o.score = me.score AND (o.updated_at < me.updated_at
                OR (o.updated_at = me.updated_at AND o.install < me.install))))) AS place,
    (SELECT COUNT(*) FROM scores o WHERE o.board = me.board) AS players
  FROM scores me WHERE me.board = ?1 AND me.install = ?2`;

const TOP_SQL = `
  SELECT player, score, updated_at FROM scores WHERE board = ?1
  ORDER BY score DESC, updated_at ASC, install ASC LIMIT ?2`;
const COUNT_SQL = "SELECT COUNT(*) AS players FROM scores WHERE board = ?1";

async function register(env: Env, ip: string, now: number): Promise<Response> {
  if (await limited(env.RL_REGISTER, ip)) return tooMany();
  const token = newToken();
  await env.DB.prepare("INSERT INTO installs (token_hash, created_at) VALUES (?1, ?2)")
    .bind(await hashToken(token), Math.floor(now / 1000))
    .run();
  return reply(200, registerOk(token));
}

async function submit(request: Request, env: Env, ip: string, now: number): Promise<Response> {
  if (await limited(env.RL_SCORES_IP, ip)) return tooMany();
  const token = bearerToken(request.headers.get("authorization"));
  if (token === null) return fail(401, "unknown_token");
  const body = await readBody(request, MAX_BODY);
  if (body === null) return fail(413, "too_large");
  const parsed = parseSubmit(body);
  if (!parsed.ok) return fail(400, parsed.reason);

  const install = await hashToken(token);
  if (await limited(env.RL_INSTALL, install)) return tooMany();
  const known = await env.DB.prepare("SELECT 1 AS found FROM installs WHERE token_hash = ?1").bind(install).first();
  if (known === null) return fail(401, "unknown_token");

  const { table, player, score } = parsed.value;
  const [, placed] = await env.DB.batch<{ place: number; players: number }>([
    env.DB.prepare(UPSERT_SQL).bind(table, install, player, score, Math.floor(now / 1000)),
    env.DB.prepare(PLACE_SQL).bind(table, install),
  ]);
  const row = placed?.results[0];
  if (row === undefined) throw new Error("the place query returned no row");
  return reply(200, submitOk(row.place, row.players));
}

async function top(url: URL, env: Env, ip: string): Promise<Response> {
  if (await limited(env.RL_READ, ip)) return tooMany();
  const table = parseTable(url.searchParams.get("board"));
  if (table === null) return fail(400, "bad_table");
  const limit = parseLimit(url.searchParams.get("limit"));
  const [rows, count] = await env.DB.batch<TopRow & { players: number }>([
    env.DB.prepare(TOP_SQL).bind(table, limit),
    env.DB.prepare(COUNT_SQL).bind(table),
  ]);
  const players = count?.results[0]?.players ?? 0;
  return reply(200, topOk(players, rows?.results ?? []));
}

/** The whole API. `now` (ms) is a parameter so the tests control the clock. */
export async function handle(request: Request, env: Env, now: number = Date.now()): Promise<Response> {
  const url = new URL(request.url);
  const ip = request.headers.get("cf-connecting-ip") ?? "unknown";
  switch (url.pathname) {
    case "/":
      return request.method === "GET" || request.method === "HEAD"
        ? reply(200, "arCade leaderboard server\n")
        : notAllowed("GET, HEAD");
    case "/v1/register":
      return request.method === "POST" ? register(env, ip, now) : notAllowed("POST");
    case "/v1/scores":
      return request.method === "POST" ? submit(request, env, ip, now) : notAllowed("POST");
    case "/v1/top":
      return request.method === "GET" ? top(url, env, ip) : notAllowed("GET");
    default:
      return fail(404, "not_found");
  }
}

export default {
  async fetch(request: Request, env: Env): Promise<Response> {
    try {
      return await handle(request, env);
    } catch (error) {
      console.error("unhandled error", error); // visible with `wrangler tail`; never sent to the client
      return fail(500, "server_error"); // the client treats this as "try again later"
    }
  },
};
