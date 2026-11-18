-- Registrations are limited by counting the installs of the last hour (see MAX_REGISTRATIONS_PER_HOUR).
-- The Workers rate limiter is only best effort across connections; this count is exact.
CREATE INDEX installs_created ON installs (created_at);
