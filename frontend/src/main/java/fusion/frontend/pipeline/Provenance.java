package fusion.frontend.pipeline;

import java.util.Objects;

/** What a batch's header records: the frontend's version and the session's id, never its name. */
public record Provenance(String frontendVersion, String sessionId) {
    public Provenance {
        Objects.requireNonNull(frontendVersion, "frontendVersion");
        Objects.requireNonNull(sessionId, "sessionId");
    }
}
