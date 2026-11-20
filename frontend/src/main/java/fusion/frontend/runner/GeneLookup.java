package fusion.frontend.runner;

import java.nio.file.Path;
import java.time.Duration;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Optional;
import java.util.concurrent.ConcurrentHashMap;

import fusion.frontend.pipeline.BatchText;
import fusion.frontend.pipeline.InputValue;
import fusion.frontend.pipeline.Json;
import tools.jackson.databind.JsonNode;

/**
 * Gene lookups (S8.12, 10.C15): a symbol map's id-label pairs come from section 02's
 * {@code labels [map=<file>]} line through {@code fusion --query} (2B.6), so the frontend reads no map
 * format itself; they are held in memory once per map, and cached on disk by {@link QueryCache}. A
 * search ranks exact id or symbol matches first, then prefixes, then substrings, each class by symbol
 * then id, and returns the top 20. FuSION 1 rescanned its map on every keystroke and kept a typed
 * text with no match ({@code Linux/frontend/src/IdEntryPanel.java:406-503}; S8.12).
 */
public final class GeneLookup {
    public record Gene(String id, String label) {}

    static final int TOP = 20;

    private record Pairs(List<Gene> genes, Map<String, String> labels) {}

    private final QueryCache cache;
    private final Map<String, Pairs> memory = new ConcurrentHashMap<>();

    public GeneLookup(BackendClient backend, Duration timeout, Path dataDir) {
        this.cache = new QueryCache(backend, timeout, dataDir);
    }

    /** Every pair of the map, the shipped symbol map when none is given. */
    public List<Gene> genes(Optional<Path> map) {
        return pairs(map).genes();
    }

    public boolean known(Optional<Path> map, String id) {
        return pairs(map).labels().containsKey(id);
    }

    /** Each id's label, for showing ids with their symbols (S8.14's preview). */
    public Map<String, String> labels(Optional<Path> map) {
        return pairs(map).labels();
    }

    public List<Gene> search(Optional<Path> map, String text) {
        String q = text.strip().toLowerCase(Locale.ROOT);
        if (q.isEmpty()) {
            return List.of();
        }
        List<List<Gene>> classes = List.of(new ArrayList<>(), new ArrayList<>(), new ArrayList<>());
        for (Gene g : pairs(map).genes()) {
            String id = g.id().toLowerCase(Locale.ROOT);
            String label = g.label().toLowerCase(Locale.ROOT);
            if (id.equals(q) || label.equals(q)) {
                classes.get(0).add(g);
            } else if (id.startsWith(q) || label.startsWith(q)) {
                classes.get(1).add(g);
            } else if (id.contains(q) || label.contains(q)) {
                classes.get(2).add(g);
            }
        }
        Comparator<Gene> order = Comparator.comparing(Gene::label).thenComparing(Gene::id);
        List<Gene> out = new ArrayList<>();
        for (List<Gene> c : classes) {
            c.sort(order);
            for (Gene g : c) {
                if (out.size() == TOP) {
                    return out;
                }
                out.add(g);
            }
        }
        return out;
    }

    private Pairs pairs(Optional<Path> map) {
        String line = map.map(p -> "labels map=" + BatchText.value(new InputValue.PathValue(p.toAbsolutePath())))
                .orElse("labels");
        String key = cache.key(line, map.map(List::of).orElse(List.of()));
        return memory.computeIfAbsent(key, k -> {
            List<Gene> genes = parse(cache.answer(k, line, GeneLookup::parse));
            Map<String, String> labels = new HashMap<>();
            genes.forEach(g -> labels.putIfAbsent(g.id(), g.label()));
            return new Pairs(List.copyOf(genes), Map.copyOf(labels));
        });
    }

    /**
     * The pairs of a {@code labels} answer: fusion's {@code {"map": ..., "labels": [[id, label], ...]}},
     * or a list under {@code pairs}, of {@code [id, label]} arrays or {@code {"id", "label"}} objects.
     */
    static List<Gene> parse(JsonNode answer) {
        JsonNode list = answer.has("labels") ? answer.get("labels") : answer.has("pairs") ? answer.get("pairs") : answer;
        if (!list.isArray()) {
            throw new RunnerException("lookup.bad_answer", "fusion's labels answer holds no pairs: "
                    + Json.compact(answer).substring(0, Math.min(200, Json.compact(answer).length())));
        }
        List<Gene> out = new ArrayList<>();
        for (JsonNode p : list) {
            if (p.isArray() && p.size() >= 1) {
                String id = p.get(0).asString();
                out.add(new Gene(id, p.size() > 1 ? p.get(1).asString() : id));
            } else if (p.isObject()) {
                String id = Json.requiredText(p, "id", "a labels pair");
                String label = Json.text(p, "label").or(() -> Json.text(p, "symbol"))
                        .or(() -> Json.text(p, "name")).orElse(id);
                out.add(new Gene(id, label));
            }
        }
        return out;
    }
}
