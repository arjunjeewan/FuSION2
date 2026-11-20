package fusion.frontend.sessions;

import java.util.Objects;
import java.util.UUID;

/**
 * A session's stable id (S8.04): a random UUID. FuSION 1 addressed sessions by their position in a
 * per-user array, so a save could overwrite the neighbouring session ({@code UserInput.java:352}).
 */
public record SessionId(UUID value) {
    public SessionId {
        Objects.requireNonNull(value, "value");
    }

    @Override
    public String toString() {
        return value.toString();
    }
}
