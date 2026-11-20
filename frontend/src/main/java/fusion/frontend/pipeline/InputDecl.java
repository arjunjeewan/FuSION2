package fusion.frontend.pipeline;

import java.util.List;
import java.util.Objects;
import java.util.Optional;
import java.util.OptionalDouble;

/**
 * One declared input of a pipeline (section 10, The pipeline document): the values a session holds
 * (S8.07 to S8.16), with what the screens need to show and check them.
 *
 * @param sweepable whether the input may be swept (decision 0018, 10.C21)
 * @param remember  whether each user's new sessions copy its last value (S8.31's {@code kgml_dir})
 */
public record InputDecl(String name, ArgType type, boolean list, Optional<InputHint> hint, boolean required,
                        Optional<InputValue> defaultValue, Optional<String> unit, OptionalDouble min,
                        OptionalDouble max, List<String> choices, boolean sweepable, boolean remember,
                        String label, String group, String help) {
    public InputDecl {
        Objects.requireNonNull(name, "name");
        Objects.requireNonNull(type, "type");
        Objects.requireNonNull(hint, "hint");
        Objects.requireNonNull(defaultValue, "defaultValue");
        Objects.requireNonNull(unit, "unit");
        Objects.requireNonNull(min, "min");
        Objects.requireNonNull(max, "max");
        choices = List.copyOf(choices);
        label = label == null || label.isEmpty() ? name : label;
        group = group == null ? "" : group;
        help = help == null ? "" : help;
    }

    /** The value a new session starts with: the default, or unset. */
    public InputValue initialValue() {
        return defaultValue.orElse(InputValue.UNSET);
    }

    /** The {@code let} variable the generator writes for this input: {@code lfc} becomes {@code $LFC}. */
    public String variable() {
        return "$" + name.toUpperCase(java.util.Locale.ROOT);
    }
}
