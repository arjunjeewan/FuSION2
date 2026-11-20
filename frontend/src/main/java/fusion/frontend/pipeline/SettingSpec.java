package fusion.frontend.pipeline;

import java.util.Objects;
import java.util.Optional;

/** A run-level setting, keyed {@code <module>.<name>}, that a {@code run} line takes (1.16, 2B.2). */
public record SettingSpec(String key, ArgType type, Need need, Optional<String> defaultValue, String help) {
    public SettingSpec {
        Objects.requireNonNull(key, "key");
        Objects.requireNonNull(type, "type");
        Objects.requireNonNull(need, "need");
        Objects.requireNonNull(defaultValue, "defaultValue");
        help = help == null ? "" : help;
    }
}
