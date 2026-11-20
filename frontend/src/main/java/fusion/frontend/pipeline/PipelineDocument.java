package fusion.frontend.pipeline;

import java.util.List;
import java.util.Objects;
import java.util.Optional;

/**
 * A pipeline: declared inputs and blocks (section 10). The default one ships with the backend as
 * {@code share/fusion/pipelines/standard.json} (10.C12); an edited copy lives in its session and
 * records its origin, such as {@code standard@0.1.0}.
 */
public record PipelineDocument(int format, String origin, List<InputDecl> inputs, List<Block> blocks) {
    /** The format this frontend reads and writes. */
    public static final int FORMAT = 1;

    public PipelineDocument {
        Objects.requireNonNull(origin, "origin");
        inputs = List.copyOf(inputs);
        blocks = List.copyOf(blocks);
    }

    public Optional<InputDecl> input(String name) {
        for (InputDecl d : inputs) {
            if (d.name().equals(name)) {
                return Optional.of(d);
            }
        }
        return Optional.empty();
    }

    public Optional<Block> block(String id) {
        for (Block b : blocks) {
            if (b.id().equals(id)) {
                return Optional.of(b);
            }
        }
        return Optional.empty();
    }
}
