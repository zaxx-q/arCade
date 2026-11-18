// The parts of the Cloudflare runtime this Worker uses, as small structural types. The real D1 database and
// rate limiter satisfy them, and the tests satisfy them with a bun:sqlite shim and a fake limiter.

export interface D1Result<T = unknown> {
  results: T[];
  success: boolean;
  meta: Record<string, unknown>;
}

export interface D1PreparedStatement {
  bind(...values: unknown[]): D1PreparedStatement;
  first<T = unknown>(): Promise<T | null>;
  all<T = unknown>(): Promise<D1Result<T>>;
  run(): Promise<D1Result>;
}

export interface D1Database {
  prepare(sql: string): D1PreparedStatement;
  /** Runs the statements in order, as one transaction. */
  batch<T = unknown>(statements: D1PreparedStatement[]): Promise<D1Result<T>[]>;
}

export interface RateLimiter {
  limit(options: { key: string }): Promise<{ success: boolean }>;
}

export interface Env {
  DB: D1Database;
  RL_REGISTER: RateLimiter; // key: client IP
  RL_INSTALL: RateLimiter; // key: hash of the install token
  RL_SCORES_IP: RateLimiter; // key: client IP
  RL_READ: RateLimiter; // key: client IP
}
