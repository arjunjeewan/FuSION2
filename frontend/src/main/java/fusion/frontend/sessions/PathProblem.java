package fusion.frontend.sessions;

import java.nio.file.Path;
import java.util.Objects;

/** A path-valued input that fails the quick check when a session opens (S8.05, S8.30). */
public record PathProblem(String input, Path path, Kind kind) {
    public enum Kind { MISSING, UNREADABLE, NOT_A_FILE, NOT_A_DIRECTORY, CHANGED_SINCE_LAST_RUN }

    public PathProblem {
        Objects.requireNonNull(input, "input");
        Objects.requireNonNull(path, "path");
        Objects.requireNonNull(kind, "kind");
    }
}
