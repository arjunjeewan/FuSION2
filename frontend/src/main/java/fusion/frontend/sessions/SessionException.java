package fusion.frontend.sessions;

import java.io.Serial;

/** A refused session operation, with a dotted code and a message for the screen. */
public class SessionException extends RuntimeException {
    @Serial
    private static final long serialVersionUID = 1L;

    private final String code;

    public SessionException(String code, String message) {
        super(message);
        this.code = code;
    }

    public String code() {
        return code;
    }

    public static final class InvalidSessionName extends SessionException {
        @Serial
        private static final long serialVersionUID = 1L;

        public InvalidSessionName(String message) {
            super("session.invalid_name", message);
        }
    }

    public static final class SessionNameTaken extends SessionException {
        @Serial
        private static final long serialVersionUID = 1L;

        public SessionNameTaken(String name) {
            super("session.name_taken", "you already have a session called " + name);
        }
    }

    /** The session changed since the version the change was based on: reload and try again. */
    public static final class VersionConflict extends SessionException {
        @Serial
        private static final long serialVersionUID = 1L;

        public VersionConflict(long based, long current) {
            super("session.version_conflict", "the session changed elsewhere (version " + current
                    + ", not " + based + "); reopen it and apply the change again");
        }
    }

    public static final class SessionBusy extends SessionException {
        @Serial
        private static final long serialVersionUID = 1L;

        public SessionBusy(String message) {
            super("session.busy", message);
        }
    }

    public static final class SessionNotFound extends SessionException {
        @Serial
        private static final long serialVersionUID = 1L;

        public SessionNotFound(String id) {
            super("session.not_found", "no session " + id);
        }
    }

    public static final class InvalidChange extends SessionException {
        @Serial
        private static final long serialVersionUID = 1L;

        public InvalidChange(String message) {
            super("session.invalid_change", message);
        }
    }

    /** The store cannot be read or written; it keeps its previous state (atomic replace). */
    public static final class StoreFailed extends SessionException {
        @Serial
        private static final long serialVersionUID = 1L;

        public StoreFailed(String message, Throwable cause) {
            super("session.store", message);
            initCause(cause);
        }
    }
}
