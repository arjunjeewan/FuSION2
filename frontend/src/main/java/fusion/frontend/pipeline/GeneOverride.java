package fusion.frontend.pipeline;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Optional;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;

/**
 * A gene's overrides: at most one value in each group (S8.15, entry 165). FuSION 1 kept them as
 * lower-cased display lines that never reached the solve ({@code ThresholdFilterPanel.java:169-201},
 * {@code ScriptFile.java:54-58}); here the gene is a bare id and every kind reaches its file.
 */
public record GeneOverride(String geneId, Optional<Observation> observation,
                           Optional<Activity> activity, Optional<Relaxation> relaxation) {
    public GeneOverride {
        Objects.requireNonNull(geneId, "geneId");
        Objects.requireNonNull(observation, "observation");
        Objects.requireNonNull(activity, "activity");
        Objects.requireNonNull(relaxation, "relaxation");
    }

    /**
     * The override kinds this gene belongs to, as file names without {@code .txt}:
     * {@code confirmed_up}, {@code not_de}, {@code confirmed_down}, {@code active},
     * {@code inactive} and {@code nonrelaxed} (section 10, Files this section writes).
     */
    public List<String> kinds() {
        List<String> kinds = new ArrayList<>(3);
        observation.ifPresent(o -> kinds.add(Wire.name(o)));
        activity.ifPresent(a -> kinds.add(Wire.name(a)));
        relaxation.ifPresent(r -> kinds.add(Wire.name(r)));
        return kinds;
    }

    /** True when no group holds a value. */
    public boolean isEmpty() {
        return observation.isEmpty() && activity.isEmpty() && relaxation.isEmpty();
    }

    /** Every override kind, in the order the files are listed. */
    public static final List<String> KINDS =
            List.of("confirmed_up", "confirmed_down", "not_de", "active", "inactive", "nonrelaxed");

    /**
     * The genes of each kind that has any, sorted, without repeats: the lines of
     * {@code overrides/<kind>.txt}, one id per line (S8.15). A gene id with a control character, or
     * a blank one, is refused.
     */
    public static Map<String, List<String>> idsByKind(List<GeneOverride> overrides) {
        Map<String, Set<String>> byKind = new TreeMap<>();
        for (GeneOverride g : overrides) {
            String id = g.geneId().strip();
            if (id.isEmpty() || id.codePoints().anyMatch(cp -> Character.isWhitespace(cp) || BatchText.isControl(cp))) {
                throw new PipelineException("override.bad_gene_id", "an override needs one bare gene id, not '"
                        + g.geneId() + "'");
            }
            for (String kind : g.kinds()) {
                byKind.computeIfAbsent(kind, k -> new TreeSet<>()).add(id);
            }
        }
        Map<String, List<String>> out = new TreeMap<>();
        byKind.forEach((kind, ids) -> out.put(kind, List.copyOf(ids)));
        return out;
    }
}
