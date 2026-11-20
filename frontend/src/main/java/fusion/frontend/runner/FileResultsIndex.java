package fusion.frontend.runner;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Optional;
import java.util.function.BiFunction;

import fusion.frontend.accounts.AccountId;
import fusion.frontend.pipeline.Json;
import tools.jackson.databind.JsonNode;

/**
 * A run's results (S8.29, S6.19): its {@code run.json} (E1), with section 07's results summary
 * (2B.7), and any file of its run directory, never one outside it, compared by real paths, which
 * also covers Windows' case-insensitive paths (section 10, Platform notes). FuSION 1 showed no
 * result: users found the output folder by hand (S8.29).
 */
public final class FileResultsIndex implements ResultsIndex {
    private final BiFunction<AccountId, RunId, Optional<RunRecord>> records;

    public FileResultsIndex(BiFunction<AccountId, RunId, Optional<RunRecord>> records) {
        this.records = records;
    }

    @Override
    public Optional<JsonNode> manifest(AccountId who, RunId run) {
        return file(who, run, "run.json").flatMap(f -> {
            try {
                return Optional.of(Json.parse(Files.readString(f), f.toString()));
            } catch (IOException | RuntimeException e) {
                return Optional.empty();
            }
        });
    }

    @Override
    public Optional<Path> file(AccountId who, RunId run, String relativePath) {
        Optional<Path> dir = records.apply(who, run).flatMap(RunRecord::runDir);
        if (dir.isEmpty() || relativePath.isEmpty()) {
            return Optional.empty();
        }
        try {
            Path root = dir.get().toRealPath();
            Path candidate = root.resolve(relativePath).normalize();
            if (!candidate.startsWith(root) || !Files.exists(candidate)) {
                return Optional.empty();
            }
            Path real = candidate.toRealPath();
            return real.startsWith(root) ? Optional.of(real) : Optional.empty();
        } catch (IOException | RuntimeException e) {
            return Optional.empty();
        }
    }
}
