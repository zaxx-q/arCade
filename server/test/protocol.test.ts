import { describe, expect, test } from "bun:test";
import {
  GAMES,
  MAX_NAME,
  TOP_MAX,
  bearerToken,
  errBody,
  hashToken,
  newToken,
  parseLimit,
  parseSubmit,
  parseTable,
  submitOk,
  topOk,
  validPlayer,
} from "../src/protocol";

describe("parseTable", () => {
  test("accepts exactly the known games", () => {
    for (const id of GAMES.keys()) expect(parseTable(id)).toBe(id);
  });

  test("refuses everything else", () => {
    for (const bad of [
      null,
      "",
      "nope",
      "LOOP_LAP",
      " loop_lap",
      "loop_lap ",
      "loop_lap:Easy", // per-mode boards are not accepted yet
      "beat_dodge:Dodge / Bopeebo / hard",
      "constructor", // a plain object would answer to these
      "__proto__",
      "toString",
      "x".repeat(64),
      "loop_lap\n",
    ])
      expect(parseTable(bad)).toBeNull();
  });
});

describe("validPlayer", () => {
  test("accepts printable ASCII up to 15 characters", () => {
    for (const ok of ["a", "Ana", "Budi Santoso", "x".repeat(MAX_NAME), "~!@#$%^&*()", "a b"])
      expect(validPlayer(ok)).toBe(true);
  });

  test("refuses the rest", () => {
    for (const bad of [
      "",
      "   ",
      "x".repeat(MAX_NAME + 1),
      "a|b", // the field separator
      "a\nb",
      "a\rb",
      "a\tb",
      "a\u0000b",
      "a\u007fb", // DEL
      "caf\u00e9", // not ASCII: the game's name box cannot type it
      "\u{1F600}",
    ])
      expect(validPlayer(bad)).toBe(false);
  });
});

describe("parseSubmit", () => {
  test("a good line, with or without a line end", () => {
    for (const body of ["loop_lap|Ana|42", "loop_lap|Ana|42\n", "loop_lap|Ana|42\r\n"])
      expect(parseSubmit(body)).toEqual({ ok: true, value: { table: "loop_lap", player: "Ana", score: 42 } });
  });

  test("the score must not pass what the game can give", () => {
    expect(parseSubmit("loop_lap|a|5000").ok).toBe(true);
    expect(parseSubmit("loop_lap|a|5001")).toEqual({ ok: false, reason: "bad_score" });
    expect(parseSubmit("different_color|a|399").ok).toBe(true);
    expect(parseSubmit("different_color|a|400").ok).toBe(false);
    expect(parseSubmit("beat_dodge|a|2000").ok).toBe(true);
    expect(parseSubmit("beat_dodge|a|2001").ok).toBe(false);
    expect(parseSubmit("gritty_golf|a|25000").ok).toBe(true);
    expect(parseSubmit("gritty_golf|a|25001").ok).toBe(false);
    expect(parseSubmit("loop_lap|a|0").ok).toBe(true);
  });

  test("the score must be a plain whole number", () => {
    for (const bad of ["-1", "1.5", "1e3", "0x10", "abc", "", " 5", "5 ", "+5", "1234567890", "٣"])
      expect(parseSubmit(`loop_lap|a|${bad}`)).toEqual({ ok: false, reason: "bad_score" });
  });

  test("says what is wrong", () => {
    expect(parseSubmit("nope|a|1")).toEqual({ ok: false, reason: "bad_table" });
    expect(parseSubmit("loop_lap|a\u0001|1")).toEqual({ ok: false, reason: "bad_player" });
    expect(parseSubmit("loop_lap||1")).toEqual({ ok: false, reason: "bad_player" });
    expect(parseSubmit("loop_lap|a")).toEqual({ ok: false, reason: "bad_format" });
    expect(parseSubmit("loop_lap|a|1|2")).toEqual({ ok: false, reason: "bad_format" });
    expect(parseSubmit("")).toEqual({ ok: false, reason: "bad_format" });
  });

  test("one line only: a second line is not smuggled in", () => {
    expect(parseSubmit("loop_lap|a|1\nloop_lap|b|2")).toEqual({ ok: false, reason: "bad_format" });
    expect(parseSubmit("loop_lap|a|1\n\n").ok).toBe(false);
    expect(parseSubmit("loop_lap|a|1\rx").ok).toBe(false);
  });
});

describe("parseLimit", () => {
  test("defaults and clamps to 1..TOP_MAX", () => {
    expect(parseLimit(null)).toBe(TOP_MAX);
    expect(parseLimit("abc")).toBe(TOP_MAX);
    expect(parseLimit("-3")).toBe(TOP_MAX);
    expect(parseLimit("")).toBe(TOP_MAX);
    expect(parseLimit("0")).toBe(1);
    expect(parseLimit("1")).toBe(1);
    expect(parseLimit("20")).toBe(20);
    expect(parseLimit("999999")).toBe(TOP_MAX);
    expect(parseLimit("99999999999")).toBe(TOP_MAX);
  });
});

describe("tokens", () => {
  test("a new token is URL-safe, valid for the C client (1..63 of A-Za-z0-9_-) and different every time", () => {
    const seen = new Set<string>();
    for (let i = 0; i < 200; i++) {
      const t = newToken();
      expect(t).toMatch(/^[A-Za-z0-9_-]{32}$/);
      seen.add(t);
    }
    expect(seen.size).toBe(200);
  });

  test("hashToken is SHA-256 in hex", async () => {
    expect(await hashToken("abc")).toBe("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  });

  test("bearerToken reads the header and nothing else", () => {
    expect(bearerToken("Bearer abc_DEF-123")).toBe("abc_DEF-123");
    expect(bearerToken("bearer abc")).toBe("abc"); // the scheme name is case-insensitive
    for (const bad of [null, "", "Bearer", "Bearer ", "Basic abc", "Bearer a b", "Bearer a\r\nX: 1", "abc", "Bearer " + "a".repeat(64)])
      expect(bearerToken(bad)).toBeNull();
  });
});

describe("answers", () => {
  test("have the format the C client parses", () => {
    expect(submitOk(3, 17)).toBe("ok|3|17\n");
    expect(errBody("bad_score")).toBe("err|bad_score\n");
    expect(topOk(0, [])).toBe("ok|0\n");
    expect(
      topOk(42, [
        { player: "ana", score: 900, updated_at: 1700000001 },
        { player: "budi", score: 800, updated_at: 1700000002 },
      ]),
    ).toBe("ok|42\n1|ana|900|1700000001\n2|budi|800|1700000002\n");
  });
});
