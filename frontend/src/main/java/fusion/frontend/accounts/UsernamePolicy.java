package fusion.frontend.accounts;

import java.util.Locale;
import java.util.regex.Pattern;

/**
 * Usernames (section 10, Accounts): trimmed and lower-cased with {@link Locale#ROOT}, then matched
 * against {@code [a-z][a-z0-9_-]{1,31}}. Usernames never become paths, so FuSION 1's {@code ../}
 * hole, where {@code new File(sessionsDir, username + ".json")} could leave its folder
 * ({@code Linux/frontend/src/LoginPage.java:200, 438}; S8.02), is closed by construction.
 */
public final class UsernamePolicy {
    private UsernamePolicy() {}

    public static final Pattern VALID = Pattern.compile("[a-z][a-z0-9_-]{1,31}");

    public static String normalise(String raw) {
        return raw == null ? "" : raw.strip().toLowerCase(Locale.ROOT);
    }

    public static boolean isValid(String normalised) {
        return VALID.matcher(normalised).matches();
    }
}
