package fusion.frontend.runner;

import java.util.List;

/** The answer of {@code fusion -b <batch> --check --json}. */
public record CheckReport(boolean ok, List<CheckDiagnostic> diagnostics) {
    public CheckReport {
        diagnostics = List.copyOf(diagnostics);
    }
}
