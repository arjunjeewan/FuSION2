package fusion.frontend.pipeline;

import java.io.Serial;

/**
 * A pipeline document, catalog or value that cannot be read or written, with a dotted code in the
 * style of section 09's failures (for example {@code generate.control_character}).
 */
public class PipelineException extends RuntimeException {
    @Serial
    private static final long serialVersionUID = 1L;

    private final String code;

    public PipelineException(String code, String message) {
        super(message);
        this.code = code;
    }

    public PipelineException(String code, String message, Throwable cause) {
        super(message, cause);
        this.code = code;
    }

    public String code() {
        return code;
    }
}
