package fusion.frontend.runner;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Clock;
import java.time.Duration;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.OptionalInt;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;
import java.util.UUID;

import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.Assumptions;
import org.junit.jupiter.api.BeforeAll;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.condition.DisabledOnOs;
import org.junit.jupiter.api.condition.OS;
import org.junit.jupiter.api.io.TempDir;

import fusion.frontend.accounts.Account;
import fusion.frontend.accounts.FileAccountService;
import fusion.frontend.pipeline.Catalog;
import fusion.frontend.pipeline.FrontendVersion;
import fusion.frontend.pipeline.GeneOverride;
import fusion.frontend.pipeline.InputValue;
import fusion.frontend.pipeline.Json;
import fusion.frontend.pipeline.Observation;
import fusion.frontend.pipeline.PipelineDocument;
import fusion.frontend.pipeline.PipelineProblem;
import fusion.frontend.pipeline.Severity;
import fusion.frontend.pipeline.StandardBatchGenerator;
import fusion.frontend.pipeline.StandardPipelineValidator;
import fusion.frontend.pipeline.TestFixtures;
import fusion.frontend.sessions.FileSessionStore;
import fusion.frontend.sessions.Session;
import tools.jackson.databind.JsonNode;

/**
 * Contract tests against a real {@code fusion} build (section 10, Test hooks; section 12, Frontend
 * contracts), the staged one at {@code build/dev/stage/bin/fusion}, which finds its own home beside
 * it, {@code build/dev/stage/share/fusion}, with the shipped maps and the default pipeline (11.C20):
 * the backend's command table fits the default pipeline; Submit's gene-id check, the classification
 * preview and the threshold suggestions are fusion's answers to {@code --query} (04.C8, 2B.6);
 * {@code --check}'s warnings are shown with the run and do not block Submit (section 10, Validation);
 * and FuSION 1's toy case {@code toy-hsa13-wni-tsv-b10}, run from a session through the runner, ends
 * ok with FuSION 1's frontier. They are skipped, saying why, when that build does not exist.
 */
@DisabledOnOs(OS.WINDOWS)
class RealFusionContractTest {
    private static final String TOY = "backend/tests/reference/synthetic/toy-hsa13-wni-tsv-b10";

    @TempDir
    static Path shared;

    @TempDir
    Path tmp;

    private static Path fusion;
    private static Path home;
    private static ProcessBackendClient backend;
    private static GeneLookup genes;
    private StandardPipelineLoader pipelines;
    private Account arjun;
    private FileSessionStore sessions;
    private final DefaultRunService[] runs = new DefaultRunService[1];

    /**
     * One client and one gene lookup for the class, as the app keeps them: on the dev build, fusion's
     * version probe and its symbol map's labels take most of a second each, and do not change.
     */
    @BeforeAll
    static void realFusion() {
        Path stage = TestFixtures.repository().resolve("build/dev/stage");
        fusion = stage.resolve("bin/fusion");
        Assumptions.assumeTrue(Files.isExecutable(fusion), "skipped: no fusion at " + fusion
                + "; build the backend (cmake --preset dev, then cmake --build build/dev) to run the contract tests");
        home = stage.resolve("share/fusion");
        backend = new ProcessBackendClient(fusion, Duration.ofSeconds(60), 4);
        genes = new GeneLookup(backend, Duration.ofSeconds(60), shared);
    }

    @BeforeEach
    void runner() {
        Path data = tmp.resolve("data");
        FileAccountService accounts = new FileAccountService(data, 4, Clock.systemUTC(), UUID::randomUUID);
        arjun = accounts.create("arjun", "Arjun", "password1".toCharArray());
        // The default pipeline is the one fusion's home holds, as the app finds it (10.C13).
        pipelines = new StandardPipelineLoader(() -> StandardPipelineLoader.homeOf(backend));
        sessions = new FileSessionStore(data, pipelines, accounts, () -> runs[0], Clock.systemUTC(), UUID::randomUUID);
        runs[0] = new DefaultRunService(new DefaultRunService.Settings(data, tmp.resolve("results"), fusion, 4,
                Duration.ofMillis(50), Duration.ofSeconds(60), FrontendVersion.current()), backend, sessions, accounts,
                pipelines, new StandardPipelineValidator(), new StandardBatchGenerator(), genes,
                Clock.systemDefaultZone(), UUID::randomUUID);
    }

    @AfterEach
    void stop() {
        if (runs[0] != null) {
            runs[0].close();
        }
    }

    /** FuSION 1's toy case as a session, its inputs read from the repository (pre-merged mode). */
    private Session toy(Map<String, InputValue> changes) {
        Path inputs = TestFixtures.repository().resolve(TOY + "/inputs");
        Map<String, InputValue> values = new LinkedHashMap<>(TestFixtures.toyPremergedSession());
        values.put("lfc", new InputValue.PathValue(inputs.resolve("with_node_included.tsv")));
        values.put("merged_graph", new InputValue.PathValue(inputs.resolve("report_example_with_pathways.xml")));
        values.put("results_dir", new InputValue.PathValue(tmp.resolve("results")));
        values.putAll(changes);
        Session created = sessions.create(arjun.id(), "Toy hsa1 to hsa22, FSC hsa13");
        return sessions.update(arjun.id(), created.id(), created.version(), s -> s.withInputs(values));
    }

    @Test
    void theBackendsCommandTableFitsTheDefaultPipeline() {
        BackendInfo info = backend.info();
        assertEquals(FrontendVersion.current(), info.fusionVersion());     // one VERSION for both (11.C16)
        assertEquals(home, info.home());
        assertEquals(home.resolve("data/hsa_to_gene_symbol_map.txt"), info.referenceData().get("symbols"));
        Catalog catalog = backend.catalog();
        // Every command and argument the shipped pipeline binds exists, with fitting types and kinds.
        PipelineDocument shipped = pipelines.standard();
        assertEquals(List.of(), new StandardPipelineValidator().validate(shipped, TestFixtures.egfSession(), catalog));
        assertEquals(List.of(), new StandardPipelineValidator().validate(shipped, TestFixtures.toyPremergedSession(),
                catalog));
    }

    @Test
    void submitChecksGeneIdsAgainstFusionsSymbolMap() throws Exception {
        // The shipped symbol map through section 02's labels line (2B.6), and a session's own map.
        assertTrue(genes.known(Optional.empty(), "hsa13"));
        assertFalse(genes.known(Optional.empty(), "hsa0"));
        Path own = Files.writeString(tmp.resolve("toy symbols.txt"), "hsa1\tG1\nhsa13\tG13\n");
        assertEquals(Map.of("hsa1", "G1", "hsa13", "G13"), genes.labels(Optional.of(own)));
        // Submit refuses an id the map does not hold, before any run directory exists (S8.12, S8.30).
        Session s = toy(Map.of("fsc", new InputValue.Items(List.of(new InputValue.GeneId("hsa0")))));
        RunnerException.ValidationFailed e = assertThrows(RunnerException.ValidationFailed.class,
                () -> runs[0].submit(arjun.id(), s.id(), SubmitRequest.QUICK));
        assertEquals(List.of("input.unknown_gene"), e.problems().stream().filter(PipelineProblem::isError)
                .map(PipelineProblem::code).toList());
        assertTrue(e.problems().stream().noneMatch(x -> x.code().equals("lookup.unavailable")), e.problems()::toString);
        assertFalse(Files.exists(tmp.resolve("results")));
    }

    @Test
    void thePreviewAndTheSuggestionsAreFusionsAnswers() {
        ExpressionQueries q = new ExpressionQueries(backend, Duration.ofSeconds(60), tmp.resolve("data"), genes, sessions);
        Session s = toy(Map.of());
        PipelineDocument doc = pipelines.standard();
        Catalog catalog = backend.catalog();
        ExpressionQueries.Preview p = q.preview(arjun.id(), s, s.inputs(), doc, catalog);
        assertEquals(List.of(16, 6, 0, 22), List.of(p.up(), p.down(), p.none(), p.genes().size()));

        // hsa13 Not DE: cudf classifies it none, after the thresholds (04.C4).
        Session notDe = sessions.update(arjun.id(), s.id(), s.version(), x -> x.withOverrides(List.of(
                new GeneOverride("hsa13", Optional.of(Observation.NOT_DE), Optional.empty(), Optional.empty()))));
        p = q.preview(arjun.id(), notDe, notDe.inputs(), doc, catalog);
        assertEquals(List.of(15, 6, 1), List.of(p.up(), p.down(), p.none()));
        ExpressionQueries.Gene hsa13 = p.genes().stream().filter(g -> g.id().equals("hsa13")).findFirst().orElseThrow();
        assertEquals("none", hsa13.classification());
        assertTrue(hsa13.notDe());
        assertEquals(genes.labels(Optional.empty()).get("hsa13"), hsa13.symbol());

        ExpressionQueries.Suggestions sg = q.suggest(arjun.id(), notDe, notDe.inputs(), doc, catalog);
        assertEquals(new ExpressionQueries.Row("current", 0.9, -0.9, 16, 6, 0), sg.rows().get(0));
        assertEquals(22, sg.counts().stream().mapToInt(Integer::intValue).sum());

        // Reversed thresholds: fusion refuses the line and says why (S3.07).
        Map<String, InputValue> reversed = new LinkedHashMap<>(s.inputs());
        reversed.put("up", new InputValue.Real(-0.9));
        reversed.put("down", new InputValue.Real(0.9));
        RunnerException.QueryRefused refused = assertThrows(RunnerException.QueryRefused.class,
                () -> q.preview(arjun.id(), s, reversed, doc, catalog));
        assertEquals("batch.bad_value", refused.backendCode());
    }

    @Test
    void checkWarningsAreShownWithTheRunAndDoNotBlockSubmit() throws Exception {
        // The KGML path on the network tests' synthetic pathway (S8.31), stopped after the merge: that
        // batch names no FSC, and fusion --check notes it (batch.no_fsc, 01.C17).
        Path kgml = Files.createDirectories(tmp.resolve("kgml"));
        Files.copy(TestFixtures.repository().resolve("backend/tests/unit/network/data/tst00001.xml"),
                kgml.resolve("tst00001.xml"));
        Path pathways = Files.writeString(tmp.resolve("pathways.txt"), "tst00001\n");
        Session s = toy(Map.of("network_source", new InputValue.Choice("kgml_list"),
                "kgml_list", new InputValue.PathValue(pathways), "kgml_dir", new InputValue.PathValue(kgml),
                "use_gsea", new InputValue.Flag(false)));
        RunId id = runs[0].submit(arjun.id(), s.id(), new SubmitRequest(Optional.of("network"))).get(0);
        List<PipelineProblem> warnings = runs[0].view(arjun.id(), id).orElseThrow().warnings();
        assertEquals(List.of("batch.no_fsc"), warnings.stream().map(PipelineProblem::code).toList());
        assertEquals(Severity.WARNING, warnings.get(0).severity());
        assertEquals(Optional.of("network"), warnings.get(0).block());
        RunnerFixture.await("the run to end", () -> !runs[0].view(arjun.id(), id).orElseThrow()
                .record().state().isActive());
        RunView v = runs[0].view(arjun.id(), id).orElseThrow();
        assertEquals(RunState.FINISHED, v.record().state(), () -> "the run ended " + v.record().state() + ": "
                + v.failure().map(Json::compact).orElse("") + "\n" + log(v.record().runDir().orElseThrow()));
        assertEquals(warnings, v.warnings());
    }

    @Test
    void theToyCaseRunsFromASessionAndItsFrontierIsFuSION1s() throws Exception {
        Session toy = toy(Map.of());
        RunId id = runs[0].submit(arjun.id(), toy.id(), SubmitRequest.QUICK).get(0);
        RunnerFixture.await("the toy run to end", () -> !runs[0].view(arjun.id(), id).orElseThrow()
                .record().state().isActive());
        RunView v = runs[0].view(arjun.id(), id).orElseThrow();
        Path dir = v.record().runDir().orElseThrow();
        assertEquals(RunState.FINISHED, v.record().state(), () -> "the run ended " + v.record().state() + ": "
                + v.failure().map(Json::compact).orElse("") + "\n" + log(dir));
        assertEquals(OptionalInt.of(0), v.record().osExitStatus());
        JsonNode runJson = Json.parse(Files.readString(dir.resolve("run.json")), "run.json");
        assertEquals("ok", runJson.path("status").asString());
        assertEquals(toy.id().toString(), runJson.path("labels").path("session").asString());

        // 2.0's frontier, from section 07's results.json, against FuSION 1's .dat files (S5.25):
        // a node count and an edge count per line, with and without the FSC.
        Map<String, Set<List<Integer>>> frontier = new TreeMap<>();
        JsonNode results = Json.parse(Files.readString(dir.resolve("pathz3/results.json")), "results.json");
        for (JsonNode c : results.path("cases")) {
            Set<List<Integer>> points = new TreeSet<>(RealFusionContractTest::compare);
            c.path("points").forEach(p -> points.add(List.of(p.path("n").asInt(), p.path("e").asInt())));
            frontier.put(c.path("case").asString(), points);
        }
        Path f1 = TestFixtures.repository().resolve(TOY + "/f1/run");
        assertEquals(Map.of(
                "in", dat(f1.resolve("hsa1_to_hsa22_w_hsa13_with_node_included.tsv_b10.dat")),
                "out", dat(f1.resolve("hsa1_to_hsa22_wo_hsa13_with_node_included.tsv_b10.dat"))), frontier);
        assertEquals(Optional.of("Writing explanation pages"), v.progress().stage());
    }

    private static Set<List<Integer>> dat(Path file) throws IOException {
        Set<List<Integer>> points = new TreeSet<>(RealFusionContractTest::compare);
        for (String line : Files.readAllLines(file)) {
            if (!line.isBlank()) {
                String[] cols = line.strip().split("\\s+");
                points.add(List.of(Integer.parseInt(cols[0]), Integer.parseInt(cols[1])));
            }
        }
        return points;
    }

    private static int compare(List<Integer> a, List<Integer> b) {
        int c = Integer.compare(a.get(0), b.get(0));
        return c != 0 ? c : Integer.compare(a.get(1), b.get(1));
    }

    private static String log(Path dir) {
        try {
            List<String> lines = new ArrayList<>(Files.readAllLines(dir.resolve(dir.getFileName() + ".log")));
            return String.join("\n", lines.subList(Math.max(0, lines.size() - 20), lines.size()));
        } catch (IOException e) {
            return "(no run log)";
        }
    }
}
