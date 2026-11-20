package fusion.frontend.sessions;

import java.util.List;

/** The quick checks before a session shows: stat and hash only (S8.05). */
public interface SessionChecks {
    List<PathProblem> quick(Session session);
}
