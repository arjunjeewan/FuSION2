package fusion.frontend.pipeline;

import java.util.List;
import java.util.Map;

/**
 * Turns a pipeline and a session's values into a batch file in section 01's language (S8.18 to
 * S8.20, 10.C14). A pure function: no clock, no random numbers, no I/O.
 */
public interface BatchGenerator {
    GeneratedBatch generate(PipelineDocument pipeline, Map<String, InputValue> inputs,
                            List<GeneOverride> overrides, Catalog catalog, GenerateOptions options);
}
