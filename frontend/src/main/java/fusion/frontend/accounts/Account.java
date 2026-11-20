package fusion.frontend.accounts;

import java.time.Instant;
import java.util.Objects;

/**
 * One account (10.C10): no email address, and a role kept for later. FuSION 1's account held an
 * email that nothing read and an unsalted SHA-256 ({@code Linux/frontend/src/LoginPage.java:523-538},
 * {@code 563-577}; S8.02).
 */
public record Account(AccountId id, String username, String displayName, Role role,
                      String passwordHash, Instant created, Preferences preferences) {
    public Account {
        Objects.requireNonNull(id, "id");
        Objects.requireNonNull(username, "username");
        Objects.requireNonNull(displayName, "displayName");
        Objects.requireNonNull(role, "role");
        Objects.requireNonNull(passwordHash, "passwordHash");
        Objects.requireNonNull(created, "created");
        Objects.requireNonNull(preferences, "preferences");
    }

    /** The record without its hash, for screens and logs. */
    @Override
    public String toString() {
        return "Account[" + id + ", " + username + ", " + role + "]";
    }
}
