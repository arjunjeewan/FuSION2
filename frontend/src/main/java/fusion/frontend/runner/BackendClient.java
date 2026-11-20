package fusion.frontend.runner;

import java.nio.file.Path;
import java.time.Duration;

import fusion.frontend.pipeline.CatalogProvider;
import tools.jackson.databind.JsonNode;

/**
 * The short-lived {@code fusion} calls, outside A1 since they make no run directory (1.10): an
 * argument list, never a shell; JSON on standard output; killed after a timeout; at most four at once.
 */
public interface BackendClient extends CatalogProvider {
    BackendInfo info();                                          // --version --json

    CheckReport check(Path batchFile);                           // -b <file> --check --json

    JsonNode query(String batchLine, Duration timeout);          // --query: one batch line on stdin
}
