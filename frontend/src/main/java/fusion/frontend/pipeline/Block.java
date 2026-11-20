package fusion.frontend.pipeline;

import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;

/**
 * One block of a pipeline: a catalog command with its argument bindings, an optional condition and
 * a canvas position. The position is layout only; generation ignores it.
 */
public record Block(String id, String command, Map<String, Binding> args, List<Condition> when,
                    double x, double y) {
    public Block {
        Objects.requireNonNull(id, "id");
        Objects.requireNonNull(command, "command");
        args = Collections.unmodifiableMap(new LinkedHashMap<>(args));
        when = List.copyOf(when);
    }
}
