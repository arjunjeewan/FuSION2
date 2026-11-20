package fusion.frontend.pipeline;

/** 09's {@code core::Type}, as section 01's type table and {@code fusion --commands} name it (01.C9). */
public enum ArgType {
    BOOLEAN, INTEGER, REAL, DURATION, TEXT, CHOICE, INPUT_FILE, INPUT_LIST, OUTPUT_NAME;

    /** True for the types whose values are paths. */
    public boolean isPath() {
        return this == INPUT_FILE || this == INPUT_LIST || this == OUTPUT_NAME;
    }
}
