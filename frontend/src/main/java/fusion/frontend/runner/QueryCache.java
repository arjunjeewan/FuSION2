package fusion.frontend.runner;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Duration;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

import fusion.frontend.pipeline.AtomicFiles;
import fusion.frontend.pipeline.Json;
import fusion.frontend.pipeline.Sha256;
import tools.jackson.databind.JsonNode;

/**
 * fusion's answers to {@code --query} lines (04.C8, 10.C15), cached in memory and in
 * {@code <data dir>/query-cache/}, keyed by fusion's version, the line and the SHA-256 of every file
 * the line names, so an edited input is asked again. The cache is safe to delete. Only answers fusion
 * gave are stored; a refused query is never cached.
 */
final class QueryCache {
    private final BackendClient backend;
    private final Duration timeout;
    private final Path dir;
    private final Map<String, JsonNode> memory = new ConcurrentHashMap<>();

    QueryCache(BackendClient backend, Duration timeout, Path dataDir) {
        this.backend = backend;
        this.timeout = timeout;
        this.dir = dataDir.resolve("query-cache");
    }

    /** The cache key of a line and the files it reads. */
    String key(String line, List<Path> files) {
        StringBuilder sb = new StringBuilder(backend.info().fusionVersion()).append('\n').append(line).append('\n');
        for (Path f : files) {
            try {
                sb.append(Sha256.ofFile(f)).append('\n');
            } catch (IOException e) {
                throw new RunnerException("query.input_unreadable", "cannot read " + f + ": " + e.getMessage());
            }
        }
        return Sha256.of(sb.toString());
    }

    /** The answer to a line, from the cache when its files are unchanged; {@code check} reads it first. */
    JsonNode answer(String key, String line, java.util.function.Consumer<JsonNode> check) {
        JsonNode known = memory.get(key);
        if (known != null) {
            return known;
        }
        Path file = dir.resolve(key + ".json");
        if (Files.isRegularFile(file)) {
            try {
                JsonNode cached = Json.parse(Files.readString(file), file.toString());
                check.accept(cached);
                memory.put(key, cached);
                return cached;
            } catch (IOException | RuntimeException e) {
                // a damaged entry is asked again
            }
        }
        JsonNode answer = backend.query(line, timeout);
        check.accept(answer);              // only an answer that reads is kept
        memory.put(key, answer);
        try {
            AtomicFiles.write(file, Json.compact(answer), true);
        } catch (IOException e) {
            // the cache is safe to lose
        }
        return answer;
    }
}
