package fusion.frontend.accounts;

import java.nio.file.Path;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Objects;
import java.util.Optional;

import fusion.frontend.pipeline.InputValue;

/**
 * An account's preferences (section 10, Accounts): the default results location, and the values of
 * the inputs that the default pipeline marks {@code remember}, which new sessions copy, such as the
 * KGML folder (S8.31). The Java code names no FuSION input.
 */
public record Preferences(Optional<Path> resultsDir, Map<String, InputValue> rememberedInputs) {
    public static final Preferences NONE = new Preferences(Optional.empty(), Map.of());

    public Preferences {
        Objects.requireNonNull(resultsDir, "resultsDir");
        rememberedInputs = Collections.unmodifiableMap(new LinkedHashMap<>(rememberedInputs));
    }

    /** These preferences with one remembered input changed. */
    public Preferences remember(String input, InputValue value) {
        Map<String, InputValue> m = new LinkedHashMap<>(rememberedInputs);
        m.put(input, value);
        return new Preferences(resultsDir, m);
    }
}
