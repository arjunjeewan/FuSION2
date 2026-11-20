package fusion.frontend.sessions;

import java.nio.file.Path;
import java.util.Map;

/** What the store asks of the runner, which implements it, so sessions never depend on runner (10.C11). */
public interface SessionUse {
    /** A run of the session is queued, starting, running or stopping. */
    boolean busy(SessionId session);

    /** Each input's path and SHA-256, from the {@code run.json} of the session's last run (E1). */
    Map<Path, String> lastRunInputHashes(SessionId session);
}
