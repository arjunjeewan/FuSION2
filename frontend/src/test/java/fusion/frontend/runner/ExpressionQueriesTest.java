package fusion.frontend.runner;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.time.Duration;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;

import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.condition.DisabledOnOs;
import org.junit.jupiter.api.condition.OS;
import org.junit.jupiter.api.io.TempDir;

import fusion.frontend.pipeline.GeneOverride;
import fusion.frontend.pipeline.InputValue;
import fusion.frontend.pipeline.Observation;
import fusion.frontend.pipeline.TestFixtures;
import fusion.frontend.sessions.Session;

/**
 * The inputs view's classification preview (S8.14) and threshold suggestions (S3.15 to S3.17,
 * decision 0013) through {@code fusion --query} (04.C8), against {@link FakeFusion}: the line sent is
 * the batch's classification line with the values in place and the session's Not DE file (04.C4);
 * answers are read, cached by the files' contents, and a refusal is shown, never cached.
 */
@DisabledOnOs(OS.WINDOWS)
class ExpressionQueriesTest {
    @TempDir
    Path tmp;

    private RunnerFixture f;
    private Path queries;

    /** fusion's answer for the toy case at ±0.9 with hsa13 Not DE, cut to four genes. */
    private static final String PREVIEW = """
            {"ok": true, "query": "cudf", "read": {"lines": 22, "genes": 22, "skipped": []},
             "thresholds": {"up": 0.9, "down": -0.9},
             "not_de": {"path": "/x/not_de.txt", "removed": ["hsa13"], "unchanged": [], "unknown": []},
             "n_up": 15, "n_down": 6, "n_none": 1,
             "genes": [{"id": "hsa1", "value": 1.0, "class": "up"}, {"id": "hsa13", "value": 1.0, "class": "none", "not_de": true},
                       {"id": "hsa14", "value": -1.0, "class": "down"}, {"id": "hsa22", "value": 1.0, "class": "up"}]}
            """;

    private static final String SUGGEST = """
            {"ok": true, "query": "suggest_thresholds",
             "rows": [{"criterion": "current", "up": 0.9, "down": -0.9, "n_up": 16, "n_down": 6, "n_de": 22, "conflicts": 0},
                      {"criterion": "quantile 1%", "up": 1.0, "down": -1.0, "n_up": 16, "n_down": 6, "n_de": 22, "conflicts": 0},
                      {"criterion": "SD k=2", "up": 2.235992540205948, "down": -1.3269016311150388, "n_up": 0, "n_down": 0,
                       "n_de": 0, "conflicts": 0}],
             "histogram": {"lo": -1.0, "width": 0.5, "counts": [6, 0, 0, 16]}}
            """;

    private static final String REFUSED = """
            {"ok": false, "query": "cudf", "error": {"code": "batch.bad_value", "message": "cudf: down must be below up (S3.07)"}}
            """;

    @BeforeEach
    void setUp() throws Exception {
        f = new RunnerFixture(tmp);
        queries = tmp.resolve("queries.txt");
    }

    @AfterEach
    void tearDown() {
        f.close();
    }

    private ExpressionQueries queries(String cudf, String suggest) throws Exception {
        Path catalog = Files.writeString(tmp.resolve("catalog.json"), TestFixtures.catalogText());
        Map<String, String> env = new HashMap<>();
        env.put("FAKE_FUSION_CATALOG", catalog.toString());
        env.put("FAKE_FUSION_LABELS", Files.writeString(tmp.resolve("labels.json"),
                "{\"map\": {\"path\": \"/x/map.txt\"}, \"labels\": [[\"hsa1\", \"SRC1\"], [\"hsa13\", \"FSC13\"]]}").toString());
        env.put("FAKE_FUSION_CUDF", Files.writeString(tmp.resolve("cudf.json"), cudf).toString());
        env.put("FAKE_FUSION_SUGGEST", Files.writeString(tmp.resolve("suggest.json"), suggest).toString());
        env.put("FAKE_FUSION_QUERIES", queries.toString());
        env.put("FAKE_FUSION_HOME", tmp.resolve("home").toString());
        BackendClient backend = new ProcessBackendClient(FakeFusion.launcher(Files.createTempDirectory(tmp, "bin"), env),
                Duration.ofSeconds(20), 4);
        GeneLookup genes = new GeneLookup(backend, Duration.ofSeconds(20), f.dataDir);
        return new ExpressionQueries(backend, Duration.ofSeconds(20), f.dataDir, genes, f.sessions);
    }

    private List<String> asked(String command) throws Exception {
        return Files.exists(queries) ? Files.readAllLines(queries).stream().filter(l -> l.startsWith(command + " ")
                || l.equals(command)).toList() : List.of();
    }

    private Session notDe(Session s, String id) {
        return f.sessions.update(f.arjun.id(), s.id(), s.version(), x -> x.withOverrides(List.of(
                new GeneOverride(id, Optional.of(Observation.NOT_DE), Optional.empty(), Optional.empty()))));
    }

    private Path lfc() {
        return f.root.resolve("inputs/with_node_included.tsv");
    }

    @Test
    void theThresholdsAreTheClassificationsInputs() throws Exception {
        Session s = f.toySession("toy");
        assertEquals(new ExpressionQueries.Thresholds(Optional.of("lfc"), Optional.of("up"), Optional.of("down")),
                queries(PREVIEW, SUGGEST).thresholds(TestFixtures.standard(), s.inputs(), TestFixtures.catalog()));
    }

    @Test
    void thePreviewSendsTheBatchsClassificationLineAndReadsTheAnswer() throws Exception {
        ExpressionQueries q = queries(PREVIEW, SUGGEST);
        Session s = notDe(f.toySession("toy"), "hsa13");
        Path notDe = f.sessions.overrideFile(f.arjun.id(), s.id(), "not_de").orElseThrow();
        assertEquals(List.of("hsa13"), Files.readAllLines(notDe));

        ExpressionQueries.Preview p = q.preview(f.arjun.id(), s, s.inputs(), TestFixtures.standard(), TestFixtures.catalog());
        // Section 04's cudf line in catalog order, its values in place (01.C14), quoted only when
        // needed (01.C2), and no outputs.
        assertEquals("cudf values=" + lfc() + " up=0.9 down=-0.9 not_de=" + notDe, p.line());
        assertEquals(List.of(p.line()), asked("cudf"));
        assertEquals(List.of(15, 6, 1), List.of(p.up(), p.down(), p.none()));
        assertEquals(new ExpressionQueries.Gene("hsa1", "SRC1", 1.0, "up", false), p.genes().get(0));
        assertEquals(new ExpressionQueries.Gene("hsa13", "FSC13", 1.0, "none", true), p.genes().get(1));
        assertEquals(new ExpressionQueries.Gene("hsa14", "", -1.0, "down", false), p.genes().get(2));

        // Typed thresholds are sent as typed, before they are saved (S8.14).
        Map<String, InputValue> typed = new HashMap<>(s.inputs());
        typed.put("up", new InputValue.Real(1.01));
        typed.put("down", new InputValue.Real(-1.01));
        assertTrue(q.preview(f.arjun.id(), s, typed, TestFixtures.standard(), TestFixtures.catalog()).line()
                .contains(" up=1.01 down=-1.01 "));
        assertEquals(2, asked("cudf").size());
    }

    @Test
    void answersAreCachedByTheFilesContents() throws Exception {
        ExpressionQueries q = queries(PREVIEW, SUGGEST);
        Session s = f.toySession("toy");
        String line = q.preview(f.arjun.id(), s, s.inputs(), TestFixtures.standard(), TestFixtures.catalog()).line();
        assertEquals("cudf values=" + lfc() + " up=0.9 down=-0.9", line);     // no Not DE genes, no file
        q.preview(f.arjun.id(), s, s.inputs(), TestFixtures.standard(), TestFixtures.catalog());
        assertEquals(1, asked("cudf").size());
        // A new instance reads the disk cache (10.C15); an edited values file is asked again.
        ExpressionQueries again = queries(PREVIEW, SUGGEST);
        again.preview(f.arjun.id(), s, s.inputs(), TestFixtures.standard(), TestFixtures.catalog());
        assertEquals(1, asked("cudf").size());
        Files.writeString(lfc(), "hsa99 2.0\n", StandardOpenOption.APPEND);
        again.preview(f.arjun.id(), s, s.inputs(), TestFixtures.standard(), TestFixtures.catalog());
        assertEquals(2, asked("cudf").size());
    }

    @Test
    void aRefusalIsFusionsMessageAndIsNotCached() throws Exception {
        ExpressionQueries q = queries(REFUSED, SUGGEST);
        Session s = f.toySession("toy");
        Map<String, InputValue> typed = new HashMap<>(s.inputs());
        typed.put("up", new InputValue.Real(-0.9));
        typed.put("down", new InputValue.Real(0.9));
        RunnerException.QueryRefused e = assertThrows(RunnerException.QueryRefused.class,
                () -> q.preview(f.arjun.id(), s, typed, TestFixtures.standard(), TestFixtures.catalog()));
        assertEquals("batch.bad_value", e.backendCode());
        assertEquals("cudf: down must be below up (S3.07)", e.getMessage());
        assertThrows(RunnerException.QueryRefused.class,
                () -> q.preview(f.arjun.id(), s, typed, TestFixtures.standard(), TestFixtures.catalog()));
        assertEquals(2, asked("cudf").size());
        // A threshold left empty is not sent: there is nothing to preview.
        typed.put("up", InputValue.UNSET);
        RunnerException none = assertThrows(RunnerException.class,
                () -> q.preview(f.arjun.id(), s, typed, TestFixtures.standard(), TestFixtures.catalog()));
        assertEquals("preview.thresholds", none.code());
        assertEquals(2, asked("cudf").size());
    }

    @Test
    void theSuggestionsSendTheCurrentThresholds() throws Exception {
        ExpressionQueries q = queries(PREVIEW, SUGGEST);
        Session s = notDe(f.toySession("toy"), "hsa13");
        ExpressionQueries.Suggestions sg = q.suggest(f.arjun.id(), s, s.inputs(), TestFixtures.standard(),
                TestFixtures.catalog());
        // suggest_thresholds takes the block's values and its thresholds as current_up and current_down (04.C6).
        assertEquals("suggest_thresholds values=" + lfc() + " current_up=0.9 current_down=-0.9", sg.line());
        assertEquals(List.of(sg.line()), asked("suggest_thresholds"));
        assertEquals(3, sg.rows().size());
        assertEquals(new ExpressionQueries.Row("current", 0.9, -0.9, 16, 6, 0), sg.rows().get(0));
        assertEquals(2.235992540205948, sg.rows().get(2).up());
        assertEquals(List.of(6, 0, 0, 16), sg.counts());
        assertEquals(-1.0, sg.histogramLow());
        assertEquals(0.5, sg.histogramWidth());
    }
}
