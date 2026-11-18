-- arCade leaderboard, first schema.

-- One row per install of the game. The token itself is never stored, only its SHA-256, so a leaked
-- database does not leak usable tokens.
CREATE TABLE installs (
  token_hash TEXT PRIMARY KEY,
  created_at INTEGER NOT NULL            -- unix seconds
);

-- The BEST score of each install per table (a table is a game id). A worse run is simply not stored.
CREATE TABLE scores (
  board      TEXT    NOT NULL,           -- the table: a game id such as "loop_lap"
  install    TEXT    NOT NULL,           -- installs.token_hash
  player     TEXT    NOT NULL,           -- name shown, 1..15 printable ASCII characters
  score      INTEGER NOT NULL,
  updated_at INTEGER NOT NULL,           -- unix seconds, server clock: when this best score was set
  PRIMARY KEY (board, install)
) WITHOUT ROWID;

-- The order of a table, same as the client's: higher score first, on a tie the earlier run, then install.
CREATE INDEX scores_rank ON scores (board, score DESC, updated_at ASC, install ASC);
