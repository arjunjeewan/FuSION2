package fusion.frontend.pipeline;

import java.util.List;
import java.util.Map;

/**
 * Steps 1 and 2 of section 10's validation (S8.30): the inputs against their declarations and the
 * catalog's rules, then the pipeline itself. Both are instant and need no backend or file system;
 * the runner adds the gene ids, the paths and {@code fusion --check}.
 */
public interface PipelineValidator {
    List<PipelineProblem> validate(PipelineDocument pipeline, Map<String, InputValue> inputs, Catalog catalog);
}
