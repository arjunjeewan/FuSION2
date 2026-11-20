package fusion.frontend.pipeline;

import java.util.Objects;
import java.util.Optional;

/**
 * One problem found before a run, tied to the block, argument or input it concerns (S8.30), with a
 * dotted code such as {@code input.required} or {@code pipeline.cycle}.
 */
public record PipelineProblem(Severity severity, Optional<String> block, Optional<String> argument,
                              Optional<String> input, String code, String message) {
    public PipelineProblem {
        Objects.requireNonNull(severity, "severity");
        Objects.requireNonNull(block, "block");
        Objects.requireNonNull(argument, "argument");
        Objects.requireNonNull(input, "input");
        Objects.requireNonNull(code, "code");
        Objects.requireNonNull(message, "message");
    }

    public static PipelineProblem forInput(Severity severity, String input, String code, String message) {
        return new PipelineProblem(severity, Optional.empty(), Optional.empty(), Optional.of(input), code, message);
    }

    public static PipelineProblem forBlock(Severity severity, String block, String code, String message) {
        return new PipelineProblem(severity, Optional.of(block), Optional.empty(), Optional.empty(), code, message);
    }

    public static PipelineProblem forArgument(Severity severity, String block, String argument,
                                              Optional<String> input, String code, String message) {
        return new PipelineProblem(severity, Optional.of(block), Optional.of(argument), input, code, message);
    }

    public boolean isError() {
        return severity == Severity.ERROR;
    }

    @Override
    public String toString() {
        StringBuilder where = new StringBuilder();
        block.ifPresent(b -> where.append("block ").append(b));
        argument.ifPresent(a -> where.append(where.isEmpty() ? "" : ", ").append("argument ").append(a));
        input.ifPresent(i -> where.append(where.isEmpty() ? "" : ", ").append("input ").append(i));
        return (severity == Severity.ERROR ? "error" : "warning") + ": " + message
                + (where.isEmpty() ? "" : " (" + where + ")") + " [" + code + "]";
    }
}
