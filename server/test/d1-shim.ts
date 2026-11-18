// A D1 look-alike on bun:sqlite, so the real SQL and the real handler run in `bun test` without workerd.
// D1 is SQLite too; what differs (network, replicas) is not what these tests are about. The end-to-end
// script (test/e2e.ts) checks the real thing.
import { Database } from "bun:sqlite";
import { readFileSync, readdirSync } from "node:fs";
import type { D1Database, D1PreparedStatement, D1Result } from "../src/env";

type Params = never[]; // bun's bind types are stricter than D1's `unknown`

export function d1FromSqlite(db: Database): D1Database {
  const statement = (sql: string, params: unknown[]): D1PreparedStatement => ({
    bind: (...values) => statement(sql, values),
    first: async <T>() => (db.query(sql).get(...(params as Params)) as T | null) ?? null,
    all: async <T>() => ({
      results: db.query(sql).all(...(params as Params)) as T[],
      success: true,
      meta: {},
    }),
    run: async () => {
      const result = db.query(sql).run(...(params as Params));
      return { results: [], success: true, meta: { changes: result.changes } };
    },
  });
  return {
    prepare: (sql) => statement(sql, []),
    batch: async <T>(statements: D1PreparedStatement[]) => {
      db.run("BEGIN");
      try {
        const out: D1Result<T>[] = [];
        for (const s of statements) out.push(await s.all<T>());
        db.run("COMMIT");
        return out;
      } catch (error) {
        db.run("ROLLBACK");
        throw error;
      }
    },
  };
}

/** An empty in-memory database with every migration applied, in file-name order (as wrangler does). */
export function freshDatabase(): { db: Database; d1: D1Database } {
  const db = new Database(":memory:");
  const dir = new URL("../migrations/", import.meta.url);
  for (const file of readdirSync(dir).filter((f) => f.endsWith(".sql")).sort())
    db.exec(readFileSync(new URL(file, dir), "utf8"));
  return { db, d1: d1FromSqlite(db) };
}
