package fusion.frontend.runner;

import java.util.Objects;
import java.util.Optional;

/** A Submit of the session's current pipeline (entry 195), optionally stopping after one block (0019). */
public record SubmitRequest(Optional<String> stopAfter) {
    public static final SubmitRequest QUICK = new SubmitRequest(Optional.empty());

    public SubmitRequest {
        Objects.requireNonNull(stopAfter, "stopAfter");
    }
}
