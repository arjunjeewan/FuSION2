package fusion.frontend.sessions;

import java.time.Instant;

/** A session in a list: newest change first, then name (section 10, Determinism). */
public record SessionSummary(SessionId id, String name, Instant created, Instant updated) {}
