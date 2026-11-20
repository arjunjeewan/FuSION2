package fusion.frontend.runner;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.attribute.FileTime;
import java.util.Optional;
import java.util.function.Supplier;

import fusion.frontend.pipeline.DefaultPipeline;
import fusion.frontend.pipeline.PipelineDocument;
import fusion.frontend.pipeline.PipelineException;
import fusion.frontend.pipeline.PipelineJson;

/**
 * The default pipeline (10.C12): {@code pipelines/standard.json} in FuSION's home, which
 * {@code fusion --version --json} names, or the {@code FUSION_HOME} the backend honours too (11.C20).
 * FuSION 1 read its template from the launch directory, so a GUI started elsewhere found none
 * ({@code Linux/frontend/src/ScriptFile.java:16}; S8.01, S8.20).
 */
public final class StandardPipelineLoader implements DefaultPipeline {
    private final Supplier<Optional<Path>> home;
    private PipelineDocument cached;
    private Path cachedFile;
    private FileTime cachedTime;

    public StandardPipelineLoader(Supplier<Optional<Path>> home) {
        this.home = home;
    }

    @Override
    public synchronized PipelineDocument standard() {
        Path h = home.get().orElseThrow(() -> new PipelineException("pipeline.no_home",
                "FuSION's home is not known: fusion --version --json gave none and FUSION_HOME is not set"));
        Path file = h.resolve("pipelines").resolve("standard.json");
        try {
            FileTime time = Files.getLastModifiedTime(file);
            if (cached == null || !file.equals(cachedFile) || !time.equals(cachedTime)) {
                cached = PipelineJson.read(Files.readString(file), file.toString());
                cachedFile = file;
                cachedTime = time;
            }
            return cached;
        } catch (IOException e) {
            throw new PipelineException("pipeline.standard_missing", "cannot read the default pipeline " + file
                    + ": " + e.getMessage(), e);
        }
    }

    /** The home: fusion's own answer first, then {@code FUSION_HOME}. */
    static Optional<Path> homeOf(BackendClient backend) {
        try {
            Path h = backend.info().home();
            if (!h.toString().isEmpty()) {
                return Optional.of(h);
            }
        } catch (RunnerException e) {
            // fall back to the environment
        }
        String env = System.getenv("FUSION_HOME");
        return env == null || env.isBlank() ? Optional.empty() : Optional.of(Path.of(env));
    }
}
