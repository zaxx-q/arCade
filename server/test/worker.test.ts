import type { Database } from "bun:sqlite";
import { beforeEach, describe, expect, test } from "bun:test";
import server, { handle } from "../src/index";
import type { Env, RateLimiter } from "../src/env";
import { MAX_REGISTRATIONS_PER_HOUR, TOP_MAX, hashToken } from "../src/protocol";
import { freshDatabase } from "./d1-shim";

class Limiter implements RateLimiter {
  denied = new Set<string>();
  keys: string[] = [];
  async limit({ key }: { key: string }): Promise<{ success: boolean }> {
    this.keys.push(key);
    return { success: !this.denied.has(key) };
  }
}

const IP = "203.0.113.7";
let db: Database;
let env: Env;
let rl: { register: Limiter; install: Limiter; scoresIp: Limiter; read: Limiter };
let clock: number;

beforeEach(() => {
  const fresh = freshDatabase();
  db = fresh.db;
  rl = { register: new Limiter(), install: new Limiter(), scoresIp: new Limiter(), read: new Limiter() };
  env = {
    DB: fresh.d1,
    RL_REGISTER: rl.register,
    RL_INSTALL: rl.install,
    RL_SCORES_IP: rl.scoresIp,
    RL_READ: rl.read,
  };
  clock = 1_700_000_000_000;
});

interface Reply {
  status: number;
  text: string;
  headers: Headers;
}

interface CallOptions {
  token?: string;
  auth?: string; // a raw Authorization header, to test bad ones
  body?: string;
  ip?: string;
}

/** One request. Every call happens one second after the previous one, so "earlier" is well defined. */
async function call(method: string, path: string, opts: CallOptions = {}): Promise<Reply> {
  const headers = new Headers({ "cf-connecting-ip": opts.ip ?? IP });
  if (opts.auth !== undefined) headers.set("authorization", opts.auth);
  else if (opts.token !== undefined) headers.set("authorization", `Bearer ${opts.token}`);
  const init: RequestInit = { method, headers };
  if (opts.body !== undefined) init.body = opts.body;
  clock += 1000;
  const res = await handle(new Request("https://lb.example" + path, init), env, clock);
  return { status: res.status, text: await res.text(), headers: res.headers };
}

async function register(): Promise<string> {
  const r = await call("POST", "/v1/register");
  expect(r.status).toBe(200);
  return r.text.trim().split("|")[1]!;
}

const submit = (token: string, table: string, player: string, score: number): Promise<Reply> =>
  call("POST", "/v1/scores", { token, body: `${table}|${player}|${score}\n` });

const rows = (sql: string): Record<string, unknown>[] => db.query(sql).all() as Record<string, unknown>[];
const scoreCount = (): number => (rows("SELECT COUNT(*) AS n FROM scores")[0]!.n as number) ?? 0;

describe("register", () => {
  test("hands out a token", async () => {
    const r = await call("POST", "/v1/register");
    expect(r.status).toBe(200);
    expect(r.text).toMatch(/^ok\|[A-Za-z0-9_-]{32}\n$/);
    expect(r.headers.get("content-type")).toBe("text/plain; charset=utf-8");
    expect(r.headers.get("cache-control")).toBe("no-store");
  });

  test("stores only the hash of the token", async () => {
    const token = await register();
    const stored = rows("SELECT token_hash FROM installs");
    expect(stored.length).toBe(1);
    expect(stored[0]!.token_hash).toBe(await hashToken(token));
    expect(JSON.stringify(rows("SELECT * FROM installs"))).not.toContain(token);
  });

  test("every install gets its own token", async () => {
    expect(await register()).not.toBe(await register());
  });

  test("is rate limited per IP, and a refused request stores nothing", async () => {
    rl.register.denied.add(IP);
    const r = await call("POST", "/v1/register");
    expect(r.status).toBe(429);
    expect(r.text).toBe("err|rate_limited\n");
    expect(r.headers.get("retry-after")).toBe("30");
    expect(rows("SELECT * FROM installs").length).toBe(0);
    expect(rl.register.keys).toEqual([IP]);
  });

  // The Workers limiter is only best effort across connections; this cap is counted in D1 and exact.
  const seedInstalls = (count: number, secondsAgo: number): void => {
    const at = Math.floor(clock / 1000) - secondsAgo;
    for (let i = 0; i < count; i++)
      db.query("INSERT INTO installs (token_hash, created_at) VALUES (?, ?)").run(`seed${secondsAgo}-${i}`, at);
  };
  const installCount = (): number => rows("SELECT COUNT(*) AS n FROM installs")[0]!.n as number;

  test("the whole server accepts only so many new installs an hour, and a refusal writes nothing", async () => {
    seedInstalls(MAX_REGISTRATIONS_PER_HOUR, 60);
    const r = await call("POST", "/v1/register");
    expect(r.status).toBe(429);
    expect(r.text).toBe("err|rate_limited\n");
    expect(installCount()).toBe(MAX_REGISTRATIONS_PER_HOUR);
  });

  test("the cap is exact: one below it still gets in, and that was the last one", async () => {
    seedInstalls(MAX_REGISTRATIONS_PER_HOUR - 1, 60);
    expect((await call("POST", "/v1/register")).status).toBe(200);
    expect((await call("POST", "/v1/register")).status).toBe(429);
    expect(installCount()).toBe(MAX_REGISTRATIONS_PER_HOUR);
  });

  test("only the last hour counts", async () => {
    seedInstalls(MAX_REGISTRATIONS_PER_HOUR, 3700);
    expect((await call("POST", "/v1/register")).status).toBe(200);
  });
});

describe("submit: who may", () => {
  test("no usable token: 401 unknown_token (the client registers again)", async () => {
    for (const auth of [undefined, "Basic abc", "Bearer", "Bearer a b", "Bearer notarealtoken123"]) {
      const opts: CallOptions = { body: "loop_lap|a|1\n" };
      if (auth !== undefined) opts.auth = auth;
      const r = await call("POST", "/v1/scores", opts);
      expect(r.status).toBe(401);
      expect(r.text).toBe("err|unknown_token\n");
    }
    expect(scoreCount()).toBe(0);
  });

  test("rate limits: per IP first, then per install", async () => {
    const token = await register();
    rl.scoresIp.denied.add(IP);
    expect((await submit(token, "loop_lap", "a", 1)).status).toBe(429);
    rl.scoresIp.denied.delete(IP);
    rl.install.denied.add(await hashToken(token));
    const r = await submit(token, "loop_lap", "a", 1);
    expect(r.status).toBe(429);
    expect(r.headers.get("retry-after")).toBe("30");
    expect(scoreCount()).toBe(0);
  });
});

describe("submit: ranking", () => {
  test("places and counts players", async () => {
    const a = await register();
    const b = await register();
    expect((await submit(a, "loop_lap", "Ana", 10)).text).toBe("ok|1|1\n");
    expect((await submit(b, "loop_lap", "Budi", 20)).text).toBe("ok|1|2\n");
    expect((await submit(a, "loop_lap", "Ana", 10)).text).toBe("ok|2|2\n"); // same score again
  });

  test("sending the same score twice is harmless (the client may resend after a lost answer)", async () => {
    const a = await register();
    await submit(a, "loop_lap", "Ana", 10);
    const before = rows("SELECT * FROM scores");
    await submit(a, "loop_lap", "Ana", 10);
    expect(rows("SELECT * FROM scores")).toEqual(before); // not even the timestamp moved
  });

  test("a worse run is not stored, and the answer is about the best one", async () => {
    const a = await register();
    const b = await register();
    await submit(a, "loop_lap", "Ana", 10);
    await submit(b, "loop_lap", "Budi", 7);
    const r = await submit(a, "loop_lap", "Ana", 3);
    expect(r.text).toBe("ok|1|2\n");
    expect(rows("SELECT score FROM scores WHERE player = 'Ana'")).toEqual([{ score: 10 }]);
  });

  test("a better run replaces the score, the time and the name", async () => {
    const a = await register();
    await submit(a, "loop_lap", "Ana", 10);
    const first = rows("SELECT updated_at FROM scores")[0]!.updated_at as number;
    expect((await submit(a, "loop_lap", "Ana Maria", 15)).text).toBe("ok|1|1\n");
    const row = rows("SELECT player, score, updated_at FROM scores")[0]!;
    expect(row.player).toBe("Ana Maria");
    expect(row.score).toBe(15);
    expect(row.updated_at as number).toBeGreaterThan(first);
  });

  test("a tie goes to the earlier run, like in the game", async () => {
    const a = await register();
    const b = await register();
    expect((await submit(a, "loop_lap", "Ana", 50)).text).toBe("ok|1|1\n");
    expect((await submit(b, "loop_lap", "Budi", 50)).text).toBe("ok|2|2\n");
    expect((await submit(a, "loop_lap", "Ana", 50)).text).toBe("ok|1|2\n");
  });

  test("tables are independent", async () => {
    const a = await register();
    const b = await register();
    await submit(a, "loop_lap", "Ana", 10);
    await submit(b, "loop_lap", "Budi", 20);
    expect((await submit(a, "different_color", "Ana", 5)).text).toBe("ok|1|1\n");
    expect(scoreCount()).toBe(3);
  });

  test("the highest score each game allows is accepted", async () => {
    const a = await register();
    for (const [game, max] of [["beat_dodge", 2000], ["different_color", 399], ["gritty_golf", 25000], ["loop_lap", 5000]] as const)
      expect((await submit(a, game, "Ana", max)).status).toBe(200);
  });

  test("a CRLF line end is fine", async () => {
    const a = await register();
    const r = await call("POST", "/v1/scores", { token: a, body: "loop_lap|Ana|10\r\n" });
    expect(r.text).toBe("ok|1|1\n");
  });
});

describe("submit: refusals are 4xx, final, and write nothing", () => {
  const refused: [string, string][] = [
    ["nope|a|1", "bad_table"],
    ["loop_lap:Easy|a|1", "bad_table"],
    ["constructor|a|1", "bad_table"],
    ["__proto__|a|1", "bad_table"],
    ["loop_lap|a|5001", "bad_score"],
    ["different_color|a|400", "bad_score"],
    ["loop_lap|a|-1", "bad_score"],
    ["loop_lap|a|1.5", "bad_score"],
    ["loop_lap|a|", "bad_score"],
    ["loop_lap||1", "bad_player"],
    ["loop_lap|0123456789abcdef|1", "bad_player"],
    ["loop_lap|caf\u00e9|1", "bad_player"],
    ["loop_lap|a\u0001|1", "bad_player"],
    ["loop_lap|a|1|2", "bad_format"],
    ["loop_lap|a", "bad_format"],
    ["", "bad_format"],
    ["loop_lap|a|1\nloop_lap|b|2", "bad_format"],
  ];

  test.each(refused)("%j -> 400 %s", async (body, reason) => {
    const token = await register();
    const r = await call("POST", "/v1/scores", { token, body });
    expect(r.status).toBe(400);
    expect(r.text).toBe(`err|${reason}\n`);
    expect(scoreCount()).toBe(0);
  });

  test("a body that is too large: 413, and it is not read to the end", async () => {
    const token = await register();
    const r = await call("POST", "/v1/scores", { token, body: "loop_lap|a|1\n" + "x".repeat(600) });
    expect(r.status).toBe(413);
    expect(r.text).toBe("err|too_large\n");
    expect(scoreCount()).toBe(0);
  });
});

describe("top", () => {
  test("an empty table is a valid answer", async () => {
    const r = await call("GET", "/v1/top?board=loop_lap");
    expect(r.status).toBe(200);
    expect(r.text).toBe("ok|0\n");
  });

  test("needs no token, and takes an encoded name", async () => {
    expect((await call("GET", "/v1/top?board=loop%5Flap&limit=5")).status).toBe(200);
  });

  test("a table that does not exist is a 400", async () => {
    for (const path of ["/v1/top", "/v1/top?board=", "/v1/top?board=nope", "/v1/top?board=loop_lap:Easy", "/v1/top?board=%00"]) {
      const r = await call("GET", path);
      expect(r.status).toBe(400);
      expect(r.text).toBe("err|bad_table\n");
    }
  });

  test("rows come best first with their rank, ties by the earlier run", async () => {
    const tokens = [await register(), await register(), await register(), await register()];
    await submit(tokens[0]!, "loop_lap", "low", 5);
    await submit(tokens[1]!, "loop_lap", "tie-early", 30);
    await submit(tokens[2]!, "loop_lap", "tie-late", 30);
    await submit(tokens[3]!, "loop_lap", "best", 99);
    const r = await call("GET", "/v1/top?board=loop_lap");
    const lines = r.text.trimEnd().split("\n");
    expect(lines[0]).toBe("ok|4");
    const parsed = lines.slice(1).map((l) => l.split("|"));
    expect(parsed.map((p) => p[0])).toEqual(["1", "2", "3", "4"]);
    expect(parsed.map((p) => p[1])).toEqual(["best", "tie-early", "tie-late", "low"]);
    expect(parsed.map((p) => p[2])).toEqual(["99", "30", "30", "5"]);
    for (const line of lines.slice(1)) expect(line).toMatch(/^[1-9][0-9]*\|[ -~]+\|[0-9]+\|[0-9]+$/);
    expect(Number(parsed[1]![3])).toBeLessThan(Number(parsed[2]![3])); // the stamp of the earlier run is smaller
  });

  test("limit cuts the rows, but players is the whole table", async () => {
    for (let i = 1; i <= 5; i++) await submit(await register(), "loop_lap", `p${i}`, i);
    const count = async (query: string): Promise<[string, number]> => {
      const r = await call("GET", "/v1/top?board=loop_lap" + query);
      const lines = r.text.trimEnd().split("\n");
      return [lines[0]!, lines.length - 1];
    };
    expect(await count("&limit=2")).toEqual(["ok|5", 2]);
    expect(await count("&limit=0")).toEqual(["ok|5", 1]);
    expect(await count("&limit=abc")).toEqual(["ok|5", 5]);
    expect(await count("")).toEqual(["ok|5", 5]);
  });

  test("never more than TOP_MAX rows", async () => {
    for (let i = 1; i <= TOP_MAX + 5; i++) await submit(await register(), "loop_lap", `p${i}`, i);
    const r = await call("GET", "/v1/top?board=loop_lap&limit=100000");
    const lines = r.text.trimEnd().split("\n");
    expect(lines[0]).toBe(`ok|${TOP_MAX + 5}`);
    expect(lines.length - 1).toBe(TOP_MAX);
    expect(lines[1]).toContain(`p${TOP_MAX + 5}`); // the best one
  });

  test("is rate limited per IP", async () => {
    rl.read.denied.add(IP);
    const r = await call("GET", "/v1/top?board=loop_lap");
    expect(r.status).toBe(429);
    expect(r.headers.get("retry-after")).toBe("30");
  });
});

describe("routing", () => {
  test("the front page says what this is", async () => {
    const r = await call("GET", "/");
    expect(r.status).toBe(200);
    expect(r.text).toContain("arCade");
  });

  test("wrong method: 405 with Allow", async () => {
    const cases: [string, string, string][] = [
      ["POST", "/", "GET, HEAD"],
      ["GET", "/v1/register", "POST"],
      ["GET", "/v1/scores", "POST"],
      ["POST", "/v1/top", "GET"],
      ["DELETE", "/v1/scores", "POST"],
    ];
    for (const [method, path, allow] of cases) {
      const r = await call(method, path);
      expect(r.status).toBe(405);
      expect(r.headers.get("allow")).toBe(allow);
      expect(r.text).toBe("err|method_not_allowed\n");
    }
  });

  test("anything else: 404", async () => {
    for (const path of ["/v2/top", "/v1", "/v1/admin", "/v1/scores/1", "/robots.txt"]) {
      const r = await call("GET", path);
      expect(r.status).toBe(404);
      expect(r.text).toBe("err|not_found\n");
    }
  });
});

describe("a server error", () => {
  test("is a 500 the client retries later, and does not leak what went wrong", async () => {
    const broken: Env = {
      ...env,
      DB: {
        prepare: () => {
          throw new Error("boom: secret internal detail");
        },
        batch: async () => {
          throw new Error("boom");
        },
      },
    };
    const log = console.error;
    console.error = () => {}; // the Worker logs it for `wrangler tail`; keep the test output clean
    try {
      const r = await server.fetch(new Request("https://lb.example/v1/register", { method: "POST" }), broken);
      expect(r.status).toBe(500);
      const text = await r.text();
      expect(text).toBe("err|server_error\n");
      expect(text).not.toContain("boom");
    } finally {
      console.error = log;
    }
  });
});

// The C client (core/src/lbnet.c) parses these exact shapes; if this block fails, the two sides disagree.
describe("what the C client expects", () => {
  test("register, submit and top answers", async () => {
    const reg = await call("POST", "/v1/register");
    const token = reg.text.trim().split("|")[1]!;
    expect(token).toMatch(/^[A-Za-z0-9_-]{1,63}$/); // lbn_valid_token
    const sub = await submit(token, "loop_lap", "Ana", 7);
    const m = /^ok\|([1-9][0-9]*)\|([1-9][0-9]*)\n$/.exec(sub.text); // lbn_parse_submit
    expect(m).not.toBeNull();
    expect(Number(m![1])).toBeLessThanOrEqual(Number(m![2]));
    const top = await call("GET", "/v1/top?board=loop_lap");
    expect(top.text).toMatch(/^ok\|[0-9]+\n/); // lbn_parse_top
    for (const r of [await call("POST", "/v1/scores", { token, body: "nope|a|1" }), await call("GET", "/nothing")])
      expect(r.text).toMatch(/^err\|[a-z_]+\n$/);
  });
});
