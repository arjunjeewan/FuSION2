package fusion.frontend.runner;

import java.nio.file.Path;
import java.util.Optional;

import fusion.frontend.accounts.AccountId;
import tools.jackson.databind.JsonNode;

/** A run's results (S8.29): its {@code run.json}, and the files inside its run directory only. */
public interface ResultsIndex {
    Optional<JsonNode> manifest(AccountId who, RunId run);      // run.json, with section 07's summary

    Optional<Path> file(AccountId who, RunId run, String relativePath);   // inside the run directory only
}
