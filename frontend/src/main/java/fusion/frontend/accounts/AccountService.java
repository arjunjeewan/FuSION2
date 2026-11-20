package fusion.frontend.accounts;

import java.util.Optional;

/**
 * Accounts on a shared machine (10.C2): open sign-up, salted and slow hashes (10.C9), validated
 * usernames, stored outside the repository (S8.02). Errors: {@link AccountException.InvalidUsername},
 * {@link AccountException.UsernameTaken}, {@link AccountException.InvalidPassword}.
 */
public interface AccountService {
    Account create(String username, String displayName, char[] password);   // role USER

    /** The account, or empty for an unknown user or a wrong password alike, checked against a dummy hash. */
    Optional<Account> authenticate(String username, char[] password);

    void changePassword(AccountId id, char[] current, char[] next);

    /** Command line only: {@code fusion-frontend --reset-password <username>}. */
    void resetPassword(String username, char[] next);

    void setPreferences(AccountId id, Preferences preferences);

    Optional<Account> find(AccountId id);

    /** True when no account exists yet: the sign-in screen then opens on sign-up (10.C2). */
    boolean isEmpty();
}
