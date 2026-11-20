package fusion.frontend.pipeline;

import java.util.Objects;
import java.util.Optional;

/**
 * Enables a block only when an input holds a value ({@code equals}), or, with {@code equals} empty,
 * when the input is set at all. Every condition of a block must hold.
 */
public record Condition(String input, Optional<InputValue> equals) {
    public Condition {
        Objects.requireNonNull(input, "input");
        Objects.requireNonNull(equals, "equals");
    }

    /** Whether the condition holds for this value of its input. */
    public boolean holds(InputValue value) {
        if (equals.isEmpty()) {
            return value.isGiven();
        }
        return value.isGiven() && sameValue(equals.get(), value);
    }

    /**
     * Value equality across the variants that a JSON document cannot tell apart: a literal
     * {@code "kgml_list"} equals the choice {@code kgml_list}, and {@code 10} equals {@code 10.0}.
     */
    static boolean sameValue(InputValue a, InputValue b) {
        if (a instanceof InputValue.Flag fa && b instanceof InputValue.Flag fb) {
            return fa.value() == fb.value();
        }
        if (isNumber(a) && isNumber(b)) {
            return number(a) == number(b);
        }
        return a.text().equals(b.text());
    }

    private static boolean isNumber(InputValue v) {
        return v instanceof InputValue.Int || v instanceof InputValue.Real;
    }

    private static double number(InputValue v) {
        return v instanceof InputValue.Int i ? i.value() : ((InputValue.Real) v).value();
    }
}
