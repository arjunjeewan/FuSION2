package fusion.frontend.runner;

import java.io.Serial;
import java.util.List;

import fusion.frontend.pipeline.PipelineProblem;

/** A refused run operation, with a dotted code and a message for the screen. */
public class RunnerException extends RuntimeException {
    @Serial
    private static final long serialVersionUID = 1L;

    private final String code;

    public RunnerException(String code, String message) {
        super(message);
        this.code = code;
    }

    public String code() {
        return code;
    }

    /** Submit refused: every problem, tied to its block, argument or input (S8.30). */
    public static final class ValidationFailed extends RunnerException {
        @Serial
        private static final long serialVersionUID = 1L;

        private final transient List<PipelineProblem> problems;

        public ValidationFailed(List<PipelineProblem> problems) {
            super("run.validation_failed", problems.stream().filter(PipelineProblem::isError).count()
                    + " problem(s) stop the run: " + String.join("; ", problems.stream()
                            .filter(PipelineProblem::isError).map(PipelineProblem::message).limit(3).toList()));
            this.problems = List.copyOf(problems);
        }

        public List<PipelineProblem> problems() {
            return problems;
        }
    }

    /** fusion cannot be reached, has another version (11.C16), or does not fit the default pipeline. */
    public static final class BackendUnavailable extends RunnerException {
        @Serial
        private static final long serialVersionUID = 1L;

        public BackendUnavailable(String message) {
            super("run.backend_unavailable", message);
        }

        public BackendUnavailable(String message, Throwable cause) {
            super("run.backend_unavailable", message);
            initCause(cause);
        }
    }

    public static final class NotOwner extends RunnerException {
        @Serial
        private static final long serialVersionUID = 1L;

        public NotOwner(String what) {
            super("run.not_owner", "no " + what + " of yours");
        }
    }

    public static final class NotResumable extends RunnerException {
        @Serial
        private static final long serialVersionUID = 1L;

        public NotResumable(RunState state) {
            super("run.not_resumable", "a " + state.name().toLowerCase(java.util.Locale.ROOT) + " run cannot be resumed");
        }
    }

    /**
     * fusion refused a query, answering {@code {"ok": false, "error": {"code", "message"}}}: the
     * values asked about are wrong, such as an up threshold below the down one (S3.07).
     */
    public static final class QueryRefused extends RunnerException {
        @Serial
        private static final long serialVersionUID = 1L;

        private final String backendCode;

        public QueryRefused(String backendCode, String message) {
            super("run.query_refused", message);
            this.backendCode = backendCode;
        }

        /** fusion's own code, such as {@code batch.bad_value}. */
        public String backendCode() {
            return backendCode;
        }
    }

    public static final class NotKillable extends RunnerException {
        @Serial
        private static final long serialVersionUID = 1L;

        public NotKillable(String message) {
            super("run.not_killable", message);
        }
    }
}
