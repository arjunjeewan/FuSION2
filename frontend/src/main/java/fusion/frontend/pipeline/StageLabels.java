package fusion.frontend.pipeline;

import java.util.Map;

/**
 * The stage names the screens show for each command: display labels only, so the frontend keeps them
 * while {@code fusion --commands} owns everything a batch means (01.C9). FuSION 1 guessed its stage
 * from the command echo, with these labels for its commands
 * ({@code Linux/frontend/src/Gui4Panel.java:197-210}; S1.10, S8.28); 2.0 names the stage of each
 * {@code step.started} event instead (A2).
 */
public final class StageLabels {
    private StageLabels() {}

    private static final Map<String, String> LABELS = Map.ofEntries(
            Map.entry("start", "Loading mappings and metadata"),
            Map.entry("cudf", "Processing fold-change data"),
            Map.entry("suggest_thresholds", "Suggesting thresholds"),
            Map.entry("labels", "Reading gene labels"),
            Map.entry("gsea", "Shortlisting pathways (GSEA)"),
            Map.entry("mff", "Merging pathways"),
            Map.entry("rgx", "Reading the network"),
            Map.entry("wgx", "Writing the network"),
            Map.entry("sz", "Counting the network"),
            Map.entry("add", "Adding edges"),
            Map.entry("fb_rch", "Reachability corridor"),
            Map.entry("pathz3", "Constraint solving (Z3)"),
            Map.entry("psp", "Plotting solution paths"),
            Map.entry("pex", "Writing explanation pages"),
            Map.entry("plot_pareto", "Drawing the Pareto plot"),
            Map.entry("load", "Loading a saved step"),
            Map.entry("exec", "Running a shell command"),
            Map.entry("help", "Help"));

    /** The label of a command, by its canonical name; the name itself for a command not listed. */
    public static String of(String command) {
        return LABELS.getOrDefault(command, command);
    }
}
