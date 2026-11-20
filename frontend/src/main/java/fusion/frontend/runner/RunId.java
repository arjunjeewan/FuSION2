package fusion.frontend.runner;

import java.util.Objects;
import java.util.UUID;

/** The frontend's own run id, which exists from Submit, before fusion has made one (section 10, Run records). */
public record RunId(UUID value) {
    public RunId {
        Objects.requireNonNull(value, "value");
    }

    @Override
    public String toString() {
        return value.toString();
    }
}
