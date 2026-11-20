package fusion.frontend.sessions;

import java.nio.file.Path;
import java.util.List;
import java.util.Optional;
import java.util.function.UnaryOperator;

import fusion.frontend.accounts.AccountId;

/**
 * Named per-user sessions (S8.04, S8.05; 10.C8). Errors: {@link SessionException.InvalidSessionName},
 * {@link SessionException.SessionNameTaken}, {@link SessionException.VersionConflict},
 * {@link SessionException.SessionBusy}.
 */
public interface SessionStore {
    List<SessionSummary> list(AccountId owner);       // newest change first, then name

    Session create(AccountId owner, String name);

    Optional<Session> get(AccountId owner, SessionId id);

    /** Applies one change to the version it was based on; a stale version is a conflict. */
    Session update(AccountId owner, SessionId id, long basedOnVersion, UnaryOperator<Session> change);

    void delete(AccountId owner, SessionId id);       // refused while SessionUse.busy

    /**
     * The session's current override list of one kind, {@code overrides/<kind>.txt}, when it has
     * genes of that kind: section 04's preview reads the Not DE list from it (04.C8).
     */
    Optional<Path> overrideFile(AccountId owner, SessionId id, String kind);
}
