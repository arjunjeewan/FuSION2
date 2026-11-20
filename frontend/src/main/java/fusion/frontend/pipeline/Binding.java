package fusion.frontend.pipeline;

import java.nio.file.Path;
import java.util.List;
import java.util.Objects;

/**
 * What one block argument is bound to (section 10, The pipeline document). A connection to another
 * block compiles to section 01's latest-value rule (01.C16), so the generator leaves the argument out.
 */
public sealed interface Binding {
    record Literal(InputValue value) implements Binding {
        public Literal { Objects.requireNonNull(value, "value"); }
    }

    /** An input of the pipeline, written as its {@code let} variable. */
    record Input(String name) implements Binding {
        public Input { Objects.requireNonNull(name, "name"); }
    }

    /** The value that block produces (its command's {@code produces} kind). */
    record Output(String block) implements Binding {
        public Output { Objects.requireNonNull(block, "block"); }
    }

    /** A saved step of an earlier run (decision 0019), written as {@code load run=<dir> step=<step>} (01.C6). */
    record Saved(Path runDir, String step) implements Binding {
        public Saved {
            Objects.requireNonNull(runDir, "runDir");
            Objects.requireNonNull(step, "step");
        }
    }

    /** The first option that is bound. */
    record FirstOf(List<Binding> options) implements Binding {
        public FirstOf { options = List.copyOf(options); }
    }

    /**
     * The session's override file of one kind, {@code $FUSION_BATCH_DIR/overrides/<kind>.txt}, bound
     * only when the session has genes of that kind (S8.15). Added to section 10's outline so that the
     * document, not the Java code, says which argument takes which override list.
     */
    record Overrides(String kind) implements Binding {
        public Overrides {
            if (!GeneOverride.KINDS.contains(kind)) {
                throw new PipelineException("pipeline.bad_override_kind", "unknown override kind '" + kind + "'");
            }
        }
    }
}
