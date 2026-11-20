package fusion.frontend.pipeline;

import java.util.List;
import java.util.Objects;
import java.util.Optional;
import java.util.OptionalDouble;

/**
 * One argument of a catalog command, as {@code fusion --commands} exports it (01.C9).
 *
 * @param setting the registry key {@code <module>.<name>}
 * @param list    section 01's {@code list} mark: several values, comma-separated in the named form
 * @param takes   section 01's {@code takes} mark: the kind of in-memory value the argument names,
 *                the latest of that kind by default (01.C16)
 */
public record ArgSpec(String name, String setting, ArgType type, boolean list, Optional<ValueKind> takes,
                      Need need, Optional<String> defaultValue, OptionalDouble min, OptionalDouble max,
                      List<String> choices, boolean debug, String help) {
    public ArgSpec {
        Objects.requireNonNull(name, "name");
        Objects.requireNonNull(setting, "setting");
        Objects.requireNonNull(type, "type");
        Objects.requireNonNull(takes, "takes");
        Objects.requireNonNull(need, "need");
        Objects.requireNonNull(defaultValue, "defaultValue");
        Objects.requireNonNull(min, "min");
        Objects.requireNonNull(max, "max");
        choices = List.copyOf(choices);
        help = help == null ? "" : help;
    }
}
