package fusion.frontend.runner;

import java.time.Duration;
import java.util.Collections;
import java.util.Map;
import java.util.Objects;
import java.util.Optional;
import java.util.TreeMap;

/**
 * A run's progress from its events (A2, S8.28): the stage of the latest {@code step.started}, named
 * by the catalog and mapped to its block, the solve's checks, points per case and unknown answers,
 * which point is being checked and since when, and the time elapsed.
 */
public record Progress(Optional<String> block, Optional<String> stage, int checks,
                       Map<String, Integer> pointsByCase, int unknowns,
                       Optional<String> checking, Duration elapsed) {
    public static final Progress NONE = new Progress(Optional.empty(), Optional.empty(), 0, Map.of(), 0,
            Optional.empty(), Duration.ZERO);

    public Progress {
        Objects.requireNonNull(block, "block");
        Objects.requireNonNull(stage, "stage");
        pointsByCase = Collections.unmodifiableMap(new TreeMap<>(pointsByCase));
        Objects.requireNonNull(checking, "checking");
        Objects.requireNonNull(elapsed, "elapsed");
    }
}
