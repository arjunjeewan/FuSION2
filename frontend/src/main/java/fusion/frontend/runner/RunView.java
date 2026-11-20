package fusion.frontend.runner;

import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.TreeMap;

import fusion.frontend.pipeline.PipelineProblem;
import tools.jackson.databind.JsonNode;

/**
 * What the screens show of one run: its record, progress, reported results, case statuses and
 * failure, and the warnings Submit's checks gave it, which are shown and do not block (section 10,
 * Validation: "Warnings are shown and do not block").
 */
public record RunView(RunRecord record, Progress progress, List<JsonNode> reports,
                      Map<String, String> caseStatuses, Optional<JsonNode> failure, List<PipelineProblem> warnings) {
    public RunView {
        reports = List.copyOf(reports);
        caseStatuses = Collections.unmodifiableMap(new TreeMap<>(caseStatuses));
        warnings = List.copyOf(warnings);
    }
}
