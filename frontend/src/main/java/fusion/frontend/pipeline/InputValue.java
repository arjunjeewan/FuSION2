package fusion.frontend.pipeline;

import java.math.BigDecimal;
import java.nio.file.Path;
import java.util.List;
import java.util.Objects;

/**
 * One typed value of a pipeline input, as a session stores it (S8.05, 10.C8). {@link Unset} is
 * stored as an explicit JSON null, never dropped: FuSION 1 lost keys through org.json's
 * {@code put(key, null)} (S8.05; {@code Linux/frontend/src/UserInput.java:344-407}).
 */
public sealed interface InputValue {
    record Text(String value) implements InputValue {
        public Text { Objects.requireNonNull(value, "value"); }
    }

    record Int(long value) implements InputValue {}

    record Real(double value) implements InputValue {
        public Real {
            if (!Double.isFinite(value)) {
                throw new PipelineException("input.bad_value", "a real value must be finite");
            }
        }
    }

    record PathValue(Path value) implements InputValue {
        public PathValue { Objects.requireNonNull(value, "value"); }
    }

    /** A path under the installation, written as {@code $FUSION_HOME/<relative>} (01.C18). */
    record InstallPath(String relative) implements InputValue {
        public InstallPath { Objects.requireNonNull(relative, "relative"); }
    }

    /** A bare gene id, found in the symbol map in use (S8.12). */
    record GeneId(String value) implements InputValue {
        public GeneId { Objects.requireNonNull(value, "value"); }
    }

    record Choice(String value) implements InputValue {
        public Choice { Objects.requireNonNull(value, "value"); }
    }

    record Flag(boolean value) implements InputValue {}

    /** The value of an input with the {@code list} mark. */
    record Items(List<InputValue> items) implements InputValue {
        public Items { items = List.copyOf(items); }
    }

    /** The values of a swept input (decision 0018, 10.C21); sweepable inputs only. */
    record Sweep(List<InputValue> values) implements InputValue {
        public Sweep { values = List.copyOf(values); }
    }

    /** No value; stored as null. */
    record Unset() implements InputValue {}

    InputValue UNSET = new Unset();

    default boolean isSet() {
        return !(this instanceof Unset);
    }

    /** Set, and not an empty list or blank text: an optional input given empty turns its feature off. */
    default boolean isGiven() {
        return switch (this) {
            case Unset u -> false;
            case Items l -> !l.items().isEmpty();
            case Sweep s -> !s.values().isEmpty();
            case Text t -> !t.value().isBlank();
            case GeneId g -> !g.value().isBlank();
            default -> true;
        };
    }

    /**
     * The value as plain text, for comparisons and messages: numbers in their shortest exact form
     * ({@link #formatReal}), paths as given, lists joined with commas.
     */
    default String text() {
        return switch (this) {
            case Text t -> t.value();
            case Int i -> Long.toString(i.value());
            case Real r -> formatReal(r.value());
            case PathValue p -> p.value().toString();
            case InstallPath i -> "$FUSION_HOME/" + i.relative();
            case GeneId g -> g.value();
            case Choice c -> c.value();
            case Flag f -> f.value() ? "true" : "false";
            case Items l -> String.join(",", l.items().stream().map(InputValue::text).toList());
            case Sweep s -> String.join(" ", s.values().stream().map(InputValue::text).toList());
            case Unset u -> "";
        };
    }

    /**
     * A real as the shortest decimal that reads back exactly, never through a locale: Java 21's
     * {@code Double.toString}, then {@code BigDecimal}'s {@code stripTrailingZeros().toPlainString()}
     * (section 10, Generating a batch, step 5): 1.01, -1.01, 0.0000001.
     */
    public static String formatReal(double value) {
        if (value == 0.0) {
            return "0";
        }
        return new BigDecimal(Double.toString(value)).stripTrailingZeros().toPlainString();
    }
}
