// Server side of the wire format specified in core/include/arcade/lbnet.h. Pure functions, no I/O.
//
// The game is public source and its files are plain text, so nothing that arrives here can be trusted:
// everything is checked, and the checks are the only real protection the leaderboard has.

export const MAX_NAME = 15; // LB_NAME_LEN - 1 in the C client
export const MAX_TABLE = 63; // LB_GAME_LEN - 1
export const TOP_MAX = 50; // LBN_TOP_MAX
export const MAX_BODY = 512; // bytes a request body may have

/**
 * The games the hub ranks, with the highest score each one can really reach: a bigger score is a forged
 * request. The key is the game's `id` in its GameDef. To rank a new game, add a line here and deploy BEFORE
 * releasing a client that sends it (the client throws away a score the server refuses).
 *
 * Tables are plain game ids for now. A "game:board" key (per-mode boards) is refused on purpose: board names
 * are free text, so accepting any of them would let one client create unlimited tables. When boards come
 * back, give the game a list or pattern of valid boards and extend parseTable.
 */
export const GAMES: ReadonlyMap<string, number> = new Map([
  ["beat_dodge", 2000], // perfect run = 1000 x difficulty multiplier (easy 1.0, normal 1.5, hard 2.0)
  ["different_color", 399], // the 20 x 20 board ends the game at 399
  ["gritty_golf", 25000], // 1000 per level, 25 levels; strokes only subtract
  ["loop_lap", 5000], // laps: no rule limit, this is far beyond what anyone plays
]);

export type Parsed<T> = { ok: true; value: T } | { ok: false; reason: string };

export interface Submit {
  table: string;
  player: string;
  score: number;
}

/** The table key if it is a known game, else null. */
export function parseTable(key: string | null): string | null {
  if (key === null || key.length === 0 || key.length > MAX_TABLE) return null;
  return GAMES.has(key) ? key : null; // a Map: "constructor" or "__proto__" are not games
}

/** 1..15 printable ASCII characters (what the game's name box allows) except '|', not only spaces. */
export function validPlayer(name: string): boolean {
  if (name.length < 1 || name.length > MAX_NAME) return false;
  for (let i = 0; i < name.length; i++) {
    const c = name.charCodeAt(i);
    if (c < 32 || c > 126 || c === 124) return false; // 124 = '|'
  }
  return name.trim().length > 0;
}

/** Body of POST /v1/scores: "table|player|score" with one optional line end. */
export function parseSubmit(body: string): Parsed<Submit> {
  const line = body.endsWith("\r\n") ? body.slice(0, -2) : body.endsWith("\n") ? body.slice(0, -1) : body;
  if (line.includes("\n") || line.includes("\r")) return fail("bad_format");
  const fields = line.split("|");
  if (fields.length !== 3) return fail("bad_format");
  const [tableRaw, player, scoreRaw] = fields as [string, string, string];
  const table = parseTable(tableRaw);
  if (table === null) return fail("bad_table");
  if (!validPlayer(player)) return fail("bad_player");
  if (!/^[0-9]{1,9}$/.test(scoreRaw)) return fail("bad_score"); // whole and not negative
  const score = Number(scoreRaw);
  if (score > (GAMES.get(table) ?? 0)) return fail("bad_score"); // more than the game can give
  return { ok: true, value: { table, player, score } };
}

function fail(reason: string): { ok: false; reason: string } {
  return { ok: false, reason };
}

/** The `limit` query value: missing or garbage = TOP_MAX, otherwise clamped to 1..TOP_MAX. */
export function parseLimit(raw: string | null): number {
  if (raw === null || !/^[0-9]{1,6}$/.test(raw)) return TOP_MAX;
  return Math.min(TOP_MAX, Math.max(1, Number(raw)));
}

// ---------------------------------------------------------------- tokens
/** The token of an "Authorization: Bearer <token>" header, or null. Same character set as the C client's. */
export function bearerToken(header: string | null): string | null {
  if (header === null) return null;
  const match = /^bearer ([A-Za-z0-9_-]{1,63})$/i.exec(header);
  return match?.[1] ?? null;
}

/** 24 random bytes as 32 URL-safe characters. */
export function newToken(): string {
  const bytes = crypto.getRandomValues(new Uint8Array(24));
  let raw = "";
  for (const b of bytes) raw += String.fromCharCode(b);
  return btoa(raw).replaceAll("+", "-").replaceAll("/", "_").replaceAll("=", "");
}

/** What is stored instead of the token. */
export async function hashToken(token: string): Promise<string> {
  const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(token));
  return [...new Uint8Array(digest)].map((b) => b.toString(16).padStart(2, "0")).join("");
}

// ---------------------------------------------------------------- answers
export interface TopRow {
  player: string;
  score: number;
  updated_at: number;
}

export const registerOk = (token: string): string => `ok|${token}\n`;
export const submitOk = (rank: number, players: number): string => `ok|${rank}|${players}\n`;
export const errBody = (reason: string): string => `err|${reason}\n`;

/** "ok|<players>" then one "rank|player|score|stamp" per row; ranks follow the order of the rows. */
export function topOk(players: number, rows: readonly TopRow[]): string {
  return `ok|${players}\n` + rows.map((r, i) => `${i + 1}|${r.player}|${r.score}|${r.updated_at}\n`).join("");
}
