package fusion.frontend.pipeline;

import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Optional;

/**
 * A pipeline resolved for one session's values: its enabled blocks in run order, each with the
 * bindings that apply after conditions and {@code first_of}. The runner reads it to count a run's
 * cores, to find the gene ids and paths to check before Submit (S8.30), and to name stages.
 */
public final class ResolvedPipeline {
    /** One enabled block: its command, and its bound arguments in catalog order. */
    public record ResolvedBlock(String id, CommandSpec command, Map<String, Binding> args) {
        public ResolvedBlock {
            Objects.requireNonNull(id, "id");
            Objects.requireNonNull(command, "command");
            args = Collections.unmodifiableMap(new LinkedHashMap<>(args));
        }
    }

    private final PipelinePlan plan;

    private ResolvedPipeline(PipelinePlan plan) {
        this.plan = plan;
    }

    public static ResolvedPipeline of(PipelineDocument doc, Map<String, InputValue> inputs,
                                      List<GeneOverride> overrides, Catalog catalog) {
        return new ResolvedPipeline(PipelinePlan.of(doc, inputs, GeneOverride.idsByKind(overrides).keySet(), catalog));
    }

    /** The enabled blocks in run order; empty when the pipeline has structural errors. */
    public List<ResolvedBlock> blocks() {
        return plan.order.stream().map(p -> new ResolvedBlock(p.block().id(), p.command(), p.args())).toList();
    }

    /** An input's effective value: the session's, or its declaration's default. */
    public InputValue value(String input) {
        return plan.value(input);
    }

    public Optional<InputDecl> input(String name) {
        return plan.doc.input(name);
    }

    /** The value a resolved binding gives, when it is a literal or an input. */
    public Optional<InputValue> valueOf(Binding binding) {
        return switch (binding) {
            case Binding.Literal l -> Optional.of(l.value());
            case Binding.Input in -> Optional.of(plan.value(in.name()));
            default -> Optional.empty();
        };
    }
}
