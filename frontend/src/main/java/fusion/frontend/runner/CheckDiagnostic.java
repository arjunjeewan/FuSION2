package fusion.frontend.runner;

import java.util.Objects;
import java.util.Optional;
import java.util.OptionalInt;

import fusion.frontend.pipeline.Severity;

/**
 * One diagnostic of {@code fusion --check --json} (01.C14, 09.C12, cross-section item 1.9). Section
 * 10's outline has the first five fields; {@code setting} and {@code path} are added so a
 * diagnostic maps back to its argument and to step 4's missing paths.
 */
public record CheckDiagnostic(Severity severity, int line, OptionalInt point, String code, String message,
                              Optional<String> setting, Optional<String> path) {
    public CheckDiagnostic {
        Objects.requireNonNull(severity, "severity");
        Objects.requireNonNull(point, "point");
        Objects.requireNonNull(code, "code");
        Objects.requireNonNull(message, "message");
        Objects.requireNonNull(setting, "setting");
        Objects.requireNonNull(path, "path");
    }

    public CheckDiagnostic(Severity severity, int line, OptionalInt point, String code, String message) {
        this(severity, line, point, code, message, Optional.empty(), Optional.empty());
    }
}
