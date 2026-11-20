package fusion.frontend.pipeline;

import java.util.Locale;

/**
 * The lower-case snake_case names that FuSION's JSON uses for enum values ("input_file",
 * "less_or_equal", "not_de"), in one place so every reader and writer agrees.
 */
public final class Wire {
    private Wire() {}

    /** The JSON name of an enum constant: its Java name in lower case, with {@link Locale#ROOT}. */
    public static String name(Enum<?> value) {
        return value.name().toLowerCase(Locale.ROOT);
    }

    /** The constant whose JSON name is {@code text}; a {@link PipelineException} when none is. */
    public static <E extends Enum<E>> E parse(Class<E> type, String text, String what) {
        for (E constant : type.getEnumConstants()) {
            if (name(constant).equals(text)) {
                return constant;
            }
        }
        throw new PipelineException("json.bad_value", what + ": unknown value '" + text + "'");
    }
}
