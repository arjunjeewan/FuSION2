package fusion.frontend.sessions;

import java.time.Instant;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Optional;

import fusion.frontend.accounts.AccountId;
import fusion.frontend.pipeline.GeneOverride;
import fusion.frontend.pipeline.InputValue;
import fusion.frontend.pipeline.PipelineDocument;

/**
 * One session (S8.05, 10.C8): every input a run needs, its gene overrides (S8.15), and its own
 * pipeline once the user edits it (S8.18). Runs are not listed here: each run record names its
 * session.
 */
public record Session(SessionId id, AccountId owner, String name, Instant created, Instant updated,
                      long version, Map<String, InputValue> inputs, List<GeneOverride> overrides,
                      Optional<PipelineDocument> pipeline) {
    public Session {
        Objects.requireNonNull(id, "id");
        Objects.requireNonNull(owner, "owner");
        Objects.requireNonNull(name, "name");
        Objects.requireNonNull(created, "created");
        Objects.requireNonNull(updated, "updated");
        inputs = Collections.unmodifiableMap(new LinkedHashMap<>(inputs));
        overrides = List.copyOf(overrides);
        Objects.requireNonNull(pipeline, "pipeline");
    }

    public Session withInputs(Map<String, InputValue> next) {
        return new Session(id, owner, name, created, updated, version, next, overrides, pipeline);
    }

    public Session withInput(String input, InputValue value) {
        Map<String, InputValue> next = new LinkedHashMap<>(inputs);
        next.put(input, value);
        return withInputs(next);
    }

    public Session withOverrides(List<GeneOverride> next) {
        return new Session(id, owner, name, created, updated, version, inputs, next, pipeline);
    }

    public Session withPipeline(Optional<PipelineDocument> next) {
        return new Session(id, owner, name, created, updated, version, inputs, overrides, next);
    }

    public Session withName(String next) {
        return new Session(id, owner, next, created, updated, version, inputs, overrides, pipeline);
    }
}
