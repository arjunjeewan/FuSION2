package fusion.frontend.pipeline;

import java.util.Objects;

/**
 * One cross-argument rule of a command, over two of its argument names, such as
 * {@code less(down, up)} (S3.07) or {@code less_or_equal(node_min, node_max)} (S8.11).
 */
public record Rule(RuleKind kind, String a, String b) {
    public Rule {
        Objects.requireNonNull(kind, "kind");
        Objects.requireNonNull(a, "a");
        Objects.requireNonNull(b, "b");
    }

    @Override
    public String toString() {
        return Wire.name(kind) + "(" + a + ", " + b + ")";
    }
}
