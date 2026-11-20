package fusion.frontend.pipeline;

import java.util.List;
import java.util.Objects;

/** A generated batch: its text, the text's SHA-256, its side files and its source map (10.C14). */
public record GeneratedBatch(String text, String sha256, List<SideFile> sideFiles, SourceMap sourceMap) {
    public GeneratedBatch {
        Objects.requireNonNull(text, "text");
        Objects.requireNonNull(sha256, "sha256");
        sideFiles = List.copyOf(sideFiles);
        Objects.requireNonNull(sourceMap, "sourceMap");
    }
}
