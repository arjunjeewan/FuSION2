package fusion.frontend.runner;

import java.nio.file.Path;
import java.time.Duration;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;

import fusion.frontend.accounts.AccountId;
import fusion.frontend.pipeline.ArgSpec;
import fusion.frontend.pipeline.ArgType;
import fusion.frontend.pipeline.BatchText;
import fusion.frontend.pipeline.Binding;
import fusion.frontend.pipeline.Catalog;
import fusion.frontend.pipeline.CommandSpec;
import fusion.frontend.pipeline.InputValue;
import fusion.frontend.pipeline.Json;
import fusion.frontend.pipeline.PipelineDocument;
import fusion.frontend.pipeline.ResolvedPipeline;
import fusion.frontend.pipeline.ValueKind;
import fusion.frontend.sessions.Session;
import fusion.frontend.sessions.SessionStore;
import tools.jackson.databind.JsonNode;

/**
 * The threshold preview (S8.14) and the threshold suggestions (S3.15 to S3.17, decision 0013) of the
 * inputs view, through {@code fusion --query} (04.C8). The preview sends the classification line the
 * batch will hold, the pipeline's block that makes the observations with the session's values written
 * in place and its Not DE list (04.C4), and fusion answers every gene's class exactly as {@code cudf}
 * classifies it: one parser (S3.14). The suggestions send section 04's {@code suggest_thresholds}
 * line on the same values, and the answer carries each criterion's thresholds and the histogram's bins
 * (04.C7). FuSION 1's preview was a fifth parser of its own, with rules that differed from cudf's
 * ({@code Linux/frontend/src/ThresholdFilterPanel.java:434-520}; S8.14).
 */
public final class ExpressionQueries {
    /** Section 04's suggestion command (04.C6); its {@code current_<x>} arguments take the block's {@code <x>}. */
    static final String SUGGEST = "suggest_thresholds";
    private static final String CURRENT = "current_";

    /** One gene of the preview: its class as cudf gives it, up, down or none, and its symbol. */
    public record Gene(String id, String symbol, double value, String classification, boolean notDe) {}

    public record Preview(String line, int up, int down, int none, List<Gene> genes, List<String> skipped) {}

    /** One criterion of the suggestions, with the genes it would classify (S3.15 to S3.17). */
    public record Row(String criterion, double up, double down, int nUp, int nDown, int conflicts) {}

    public record Suggestions(String line, List<Row> rows, double histogramLow, double histogramWidth,
                              List<Integer> counts) {}

    /** The inputs a suggestion sets: those bound to the classification's {@code up} and {@code down}. */
    public record Thresholds(Optional<String> values, Optional<String> up, Optional<String> down) {}

    private final QueryCache cache;
    private final GeneLookup genes;
    private final SessionStore sessions;
    private final BackendClient backend;

    public ExpressionQueries(BackendClient backend, Duration timeout, Path dataDir, GeneLookup genes,
                             SessionStore sessions) {
        this.backend = backend;
        this.cache = new QueryCache(backend, timeout, dataDir);
        this.genes = genes;
        this.sessions = sessions;
    }

    /** Which inputs hold the values file and the two thresholds of the session's classification. */
    public Thresholds thresholds(PipelineDocument doc, Map<String, InputValue> inputs, Catalog catalog) {
        ResolvedPipeline r = ResolvedPipeline.of(doc, inputs, List.of(), catalog);
        Optional<ResolvedPipeline.ResolvedBlock> b = classification(r);
        return new Thresholds(b.flatMap(x -> inputOf(x, "values")), b.flatMap(x -> inputOf(x, "up")),
                b.flatMap(x -> inputOf(x, "down")));
    }

    /** The classification preview, with the thresholds as typed and the session's Not DE list. */
    public Preview preview(AccountId who, Session session, Map<String, InputValue> inputs, PipelineDocument doc,
                           Catalog catalog) {
        ResolvedPipeline r = ResolvedPipeline.of(doc, inputs, session.overrides(), catalog);
        ResolvedPipeline.ResolvedBlock b = classification(r).orElseThrow(() -> new RunnerException(
                "preview.no_classification", "this pipeline classifies no genes, so there is nothing to preview"));
        Map<String, String> tokens = new LinkedHashMap<>();
        List<Path> files = new ArrayList<>();
        for (Map.Entry<String, Binding> e : b.args().entrySet()) {
            token(who, session, r, b.command(), e.getKey(), e.getValue(), files).ifPresent(t -> tokens.put(e.getKey(), t));
        }
        if (!tokens.containsKey("up") || !tokens.containsKey("down")) {
            throw new RunnerException("preview.thresholds", "set both thresholds to see the classification");
        }
        String line = line(b.command().name(), tokens);
        JsonNode a = cache.answer(cache.key(line, files), line, ExpressionQueries::checkPreview);
        Map<String, String> symbols = symbols(r);
        List<Gene> out = new ArrayList<>();
        for (JsonNode g : a.path("genes")) {
            String id = g.path("id").asString("");
            out.add(new Gene(id, symbols.getOrDefault(id, ""), g.path("value").asDouble(), g.path("class").asString(""),
                    g.path("not_de").asBoolean(false)));
        }
        List<String> skipped = new ArrayList<>();
        a.path("read").path("skipped").forEach(s -> skipped.add(s.isString() ? s.stringValue() : Json.compact(s)));
        return new Preview(line, a.path("n_up").asInt(), a.path("n_down").asInt(), a.path("n_none").asInt(), out, skipped);
    }

    /** Threshold suggestions on the session's values, beside its current thresholds (decision 0013). */
    public Suggestions suggest(AccountId who, Session session, Map<String, InputValue> inputs, PipelineDocument doc,
                               Catalog catalog) {
        ResolvedPipeline r = ResolvedPipeline.of(doc, inputs, session.overrides(), catalog);
        ResolvedPipeline.ResolvedBlock b = classification(r).orElseThrow(() -> new RunnerException(
                "preview.no_classification", "this pipeline classifies no genes, so there is nothing to suggest"));
        CommandSpec suggest = catalog.command(SUGGEST).orElseThrow(() -> new RunnerException(
                "preview.no_suggestions", "this fusion has no " + SUGGEST + " command"));
        List<Path> files = new ArrayList<>();
        Map<String, String> tokens = new LinkedHashMap<>();
        for (ArgSpec a : suggest.args()) {
            String from = a.name().startsWith(CURRENT) ? a.name().substring(CURRENT.length()) : a.name();
            Binding binding = b.args().get(from);
            if (binding != null && a.type() != ArgType.OUTPUT_NAME) {
                token(who, session, r, b.command(), from, binding, files).ifPresent(t -> tokens.put(a.name(), t));
            }
        }
        String line = line(suggest.name(), tokens);
        JsonNode a = cache.answer(cache.key(line, files), line, ExpressionQueries::checkSuggestions);
        List<Row> rows = new ArrayList<>();
        for (JsonNode row : a.path("rows")) {
            rows.add(new Row(row.path("criterion").asString(""), row.path("up").asDouble(), row.path("down").asDouble(),
                    row.path("n_up").asInt(), row.path("n_down").asInt(), row.path("conflicts").asInt()));
        }
        List<Integer> counts = new ArrayList<>();
        a.path("histogram").path("counts").forEach(c -> counts.add(c.asInt()));
        return new Suggestions(line, rows, a.path("histogram").path("lo").asDouble(),
                a.path("histogram").path("width").asDouble(), counts);
    }

    // ---- the line -------------------------------------------------------------------------------

    /** The block that makes the run's observations, by the catalog's effects (01's ValueKind). */
    private static Optional<ResolvedPipeline.ResolvedBlock> classification(ResolvedPipeline r) {
        return r.blocks().stream().filter(b -> b.command().produces().filter(k -> k == ValueKind.OBSERVATIONS)
                .isPresent()).findFirst();
    }

    private static Optional<String> inputOf(ResolvedPipeline.ResolvedBlock b, String arg) {
        return b.args().get(arg) instanceof Binding.Input in ? Optional.of(in.name()) : Optional.empty();
    }

    /**
     * One argument as the query line writes it: the value in place, since a query line has no
     * {@code let} (01.C14); a swept value's first point; an installation path through fusion's own home;
     * the session's override list as a file. Outputs and in-memory values are left out: a query writes
     * nothing and runs alone.
     */
    private Optional<String> token(AccountId who, Session session, ResolvedPipeline r, CommandSpec command, String arg,
                                   Binding binding, List<Path> files) {
        if (command.arg(arg).filter(a -> a.type() == ArgType.OUTPUT_NAME).isPresent()) {
            return Optional.empty();
        }
        Optional<InputValue> value = switch (binding) {
            case Binding.Literal l -> Optional.of(l.value());
            case Binding.Input in -> Optional.of(r.value(in.name()));
            case Binding.Overrides o -> sessions.overrideFile(who, session.id(), o.kind())
                    .map(p -> (InputValue) new InputValue.PathValue(p));
            default -> Optional.empty();
        };
        return value.map(v -> v instanceof InputValue.Sweep s && !s.values().isEmpty() ? s.values().get(0) : v)
                .filter(InputValue::isGiven)
                .map(v -> {
                    if (v instanceof InputValue.InstallPath i) {
                        v = new InputValue.PathValue(backend.info().home().resolve(i.relative()));
                    }
                    if (v instanceof InputValue.PathValue p) {
                        files.add(p.value());
                    }
                    return BatchText.value(v);
                });
    }

    private static String line(String command, Map<String, String> tokens) {
        StringBuilder sb = new StringBuilder(command);
        tokens.forEach((name, token) -> sb.append(' ').append(name).append('=').append(token));
        return sb.toString();
    }

    private Map<String, String> symbols(ResolvedPipeline r) {
        try {
            return genes.labels(SubmitChecks.symbolMap(r));
        } catch (RunnerException e) {
            return Map.of();        // the preview still shows the ids
        }
    }

    private static void checkPreview(JsonNode a) {
        if (!a.has("genes") || !a.has("n_up")) {
            throw new RunnerException("preview.bad_answer", "fusion's preview answer holds no classification");
        }
    }

    private static void checkSuggestions(JsonNode a) {
        if (!a.has("rows")) {
            throw new RunnerException("preview.bad_answer", "fusion's suggestions answer holds no rows");
        }
    }
}
