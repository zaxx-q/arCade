// End-to-end check of a RUNNING server (wrangler dev, or the deployed Worker):
//   bun run e2e                                  -> http://127.0.0.1:8787
//   BASE_URL=https://arcade-leaderboard.<you>.workers.dev bun run e2e
// It adds a few rows under the player name "e2e-test" to the real table of one game. After a run against the
// deployed server, remove them (README, "Moderasi"). Kept to few requests: the Worker rate-limits registrations.
export {}; // makes this file a module, so top-level await typechecks

const BASE = (process.env.BASE_URL ?? "http://127.0.0.1:8787").replace(/\/$/, "");
const GAME = "different_color"; // a game with a small, easy to bound score range
const NAME = "e2e-" + Math.random().toString(16).slice(2, 10); // a new name per run, so reruns do not collide

let failed = 0;
function check(name: string, ok: boolean, detail = ""): void {
  console.log(`${ok ? "ok  " : "FAIL"} ${name}${ok ? "" : `  -> ${detail}`}`);
  if (!ok) failed++;
}

interface Reply {
  status: number;
  text: string;
  headers: Headers;
}

async function call(method: string, path: string, opts: { token?: string; body?: string } = {}): Promise<Reply> {
  const headers: Record<string, string> = {};
  if (opts.token !== undefined) headers.authorization = `Bearer ${opts.token}`;
  const init: RequestInit = { method, headers };
  if (opts.body !== undefined) init.body = opts.body;
  const res = await fetch(BASE + path, init);
  return { status: res.status, text: await res.text(), headers: res.headers };
}

const show = (r: Reply): string => `${r.status} ${JSON.stringify(r.text)}`;

console.log(`server: ${BASE}\n`);

const front = await call("GET", "/");
check("GET / answers", front.status === 200 && front.text.includes("arCade"), show(front));

const reg = await call("POST", "/v1/register");
const token = reg.text.trim().split("|")[1] ?? "";
check("register gives a token", reg.status === 200 && /^[A-Za-z0-9_-]{1,63}$/.test(token), show(reg));

const noAuth = await call("POST", "/v1/scores", { body: `${GAME}|${NAME}|5\n` });
check("submit without a token is 401", noAuth.status === 401 && noAuth.text === "err|unknown_token\n", show(noAuth));
const badAuth = await call("POST", "/v1/scores", { token: "notarealtoken", body: `${GAME}|${NAME}|5\n` });
check("submit with an unknown token is 401", badAuth.status === 401, show(badAuth));

const first = await call("POST", "/v1/scores", { token, body: `${GAME}|${NAME}|5\n` });
const m1 = /^ok\|([1-9][0-9]*)\|([1-9][0-9]*)\n$/.exec(first.text);
check("a first score is accepted: ok|rank|players", first.status === 200 && m1 !== null, show(first));

const worse = await call("POST", "/v1/scores", { token, body: `${GAME}|${NAME}|2\n` });
check("a worse score is accepted but the place is that of the best", worse.status === 200 && worse.text === first.text, show(worse));

const better = await call("POST", "/v1/scores", { token, body: `${GAME}|${NAME}|7\n` });
const m2 = /^ok\|([1-9][0-9]*)\|([1-9][0-9]*)\n$/.exec(better.text);
check("a better score is accepted", better.status === 200 && m2 !== null && Number(m2[1]) <= Number(m1?.[1] ?? 0), show(better));

const top = await call("GET", `/v1/top?board=${GAME}&limit=50`);
const lines = top.text.trimEnd().split("\n");
const rows = lines.slice(1).map((l) => l.split("|"));
check("top: header and rows have the right shape", top.status === 200 && /^ok\|[0-9]+$/.test(lines[0] ?? "") && rows.every((r) => r.length === 4 && /^[0-9]+$/.test(r[0] ?? "") && /^[0-9]+$/.test(r[2] ?? "")), show(top));
// these need real rows: an empty or failed answer must not pass them by being vacuously true
check("top: ranks are 1, 2, 3 ... and scores never go up", top.status === 200 && rows.length > 0 && rows.every((r, i) => Number(r[0]) === i + 1 && (i === 0 || Number(r[2]) <= Number(rows[i - 1]?.[2]))), show(top));
check("top: our best score (7) is there, once", top.status === 200 && rows.filter((r) => r[1] === NAME && r[2] === "7").length === 1, show(top));
check("top: the worse and the first runs are not", top.status === 200 && rows.length > 0 && !rows.some((r) => r[1] === NAME && (r[2] === "5" || r[2] === "2")), show(top));

const refusals: [string, string, number][] = [
  ["a game that does not exist", "nope|e2e-test|1\n", 400],
  ["a score above what the game allows", `${GAME}|e2e-test|400\n`, 400],
  ["a negative score", `${GAME}|e2e-test|-1\n`, 400],
  ["a name with a control character", `${GAME}|e2e\u0001test|1\n`, 400],
  ["a name that is too long", `${GAME}|0123456789abcdef|1\n`, 400],
  ["a second line", `${GAME}|e2e-test|1\n${GAME}|x|1\n`, 400],
  ["a body that is far too large", `${GAME}|e2e-test|1\n` + "x".repeat(2000), 413],
];
for (const [name, body, status] of refusals) {
  const r = await call("POST", "/v1/scores", { token, body });
  check(`refused: ${name}`, r.status === status && r.text.startsWith("err|"), show(r));
}

const badBoard = await call("GET", "/v1/top?board=nope");
check("top of an unknown table is 400", badBoard.status === 400, show(badBoard));
check("GET on /v1/scores is 405", (await call("GET", "/v1/scores")).status === 405);
check("an unknown path is 404", (await call("GET", "/v1/nothing")).status === 404);

console.log(failed === 0 ? "\nall good" : `\n${failed} check(s) FAILED`);
process.exit(failed === 0 ? 0 : 1);
