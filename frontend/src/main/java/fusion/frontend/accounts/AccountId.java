package fusion.frontend.accounts;

import java.util.Objects;
import java.util.UUID;

/** An account's stable id; files are named by it, never by the username (S8.02). */
public record AccountId(UUID value) {
    public AccountId {
        Objects.requireNonNull(value, "value");
    }

    @Override
    public String toString() {
        return value.toString();
    }
}
