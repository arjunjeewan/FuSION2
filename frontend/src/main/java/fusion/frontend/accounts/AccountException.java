package fusion.frontend.accounts;

import java.io.Serial;

/** An account operation that is refused, with a dotted code and a message for the screen. */
public class AccountException extends RuntimeException {
    @Serial
    private static final long serialVersionUID = 1L;

    private final String code;

    public AccountException(String code, String message) {
        super(message);
        this.code = code;
    }

    public String code() {
        return code;
    }

    /** A username outside {@link UsernamePolicy#VALID}. */
    public static final class InvalidUsername extends AccountException {
        @Serial
        private static final long serialVersionUID = 1L;

        public InvalidUsername(String message) {
            super("account.invalid_username", message);
        }
    }

    public static final class UsernameTaken extends AccountException {
        @Serial
        private static final long serialVersionUID = 1L;

        public UsernameTaken(String username) {
            super("account.username_taken", "the username " + username + " is taken");
        }
    }

    /** A password outside 8 to 72 bytes, or a current password that does not match. */
    public static final class InvalidPassword extends AccountException {
        @Serial
        private static final long serialVersionUID = 1L;

        public InvalidPassword(String message) {
            super("account.invalid_password", message);
        }
    }

    public static final class UnknownAccount extends AccountException {
        @Serial
        private static final long serialVersionUID = 1L;

        public UnknownAccount(String who) {
            super("account.unknown", "no account " + who);
        }
    }

    /** The account store cannot be read or written; the store keeps its previous state. */
    public static final class StoreFailed extends AccountException {
        @Serial
        private static final long serialVersionUID = 1L;

        public StoreFailed(String message, Throwable cause) {
            super("account.store", message);
            initCause(cause);
        }
    }
}
