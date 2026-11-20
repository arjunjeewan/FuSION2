package fusion.frontend.pipeline;

import static fusion.frontend.pipeline.TestFixtures.choice;
import static fusion.frontend.pipeline.TestFixtures.egfSession;
import static fusion.frontend.pipeline.TestFixtures.integer;
import static fusion.frontend.pipeline.TestFixtures.path;
import static fusion.frontend.pipeline.TestFixtures.text;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Optional;
import java.util.OptionalInt;
import java.util.Random;
import java.util.stream.Stream;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.Arguments;
import org.junit.jupiter.params.provider.MethodSource;

/** Section 10's test hooks for {@code BatchGenerator} (10.C14, S8.18 to S8.20, S8.15, S8.17). */
class BatchGeneratorTest {
    private final BatchGenerator generator = new StandardBatchGenerator();
    private final PipelineDocument standard = TestFixtures.standard();
    private final Catalog catalog = TestFixtures.catalog();

    private GeneratedBatch generate(Map<String, InputValue> inputs, List<GeneOverride> overrides, GenerateOptions o) {
        return generator.generate(standard, inputs, overrides, catalog, o);
    }

    private GeneratedBatch run(Map<String, InputValue> inputs, List<GeneOverride> overrides) {
        return generate(inputs, overrides, GenerateOptions.run(TestFixtures.provenance()));
    }

    private String golden(String name) {
        return TestFixtures.resource("/fusion/frontend/pipeline/golden/" + name + ".batch")
                .replace("@PIPELINE_SHA256@", PipelineJson.sha256(standard));
    }

    // ---- golden files: the default pipeline with fixture sessions gives byte-identical batches ----

    static Stream<Arguments> goldenCases() {
        Map<String, InputValue> noGsea = egfSession();
        noGsea.put("use_gsea", new InputValue.Flag(false));
        noGsea.put("not_merged", InputValue.UNSET);

        Map<String, InputValue> otherMaps = egfSession();
        otherMaps.put("maps", choice("other"));
        otherMaps.put("symbols", path("/home/ks-149-u24/maps/hsa_symbols_2026.tsv"));

        Map<String, InputValue> noFsc = egfSession();
        noFsc.put("fsc", new InputValue.Items(List.of()));

        Map<String, InputValue> withFiles = egfSession();
        withFiles.put("budget", text("48h"));
        withFiles.put("solutions", integer(3));
        withFiles.put("essential_edges", path("/home/ks-149-u24/data/edge files/essential.txt"));
        withFiles.put("avoid_edges", path("/home/ks-149-u24/data/avoid_$1.txt"));

        return Stream.of(
                Arguments.of("kgml-gsea", egfSession(), TestFixtures.fosbNotDe(), Optional.empty()),
                Arguments.of("kgml-no-gsea", noGsea, List.of(), Optional.empty()),
                Arguments.of("premerged-toy", TestFixtures.toyPremergedSession(), List.of(), Optional.empty()),
                Arguments.of("other-maps", otherMaps, List.of(), Optional.empty()),
                Arguments.of("no-fsc", noFsc, List.of(), Optional.empty()),
                Arguments.of("overrides", withFiles, manyOverrides(), Optional.empty()),
                Arguments.of("stop-after-gsea", egfSession(), TestFixtures.fosbNotDe(), Optional.of("gsea")));
    }

    @ParameterizedTest(name = "{0}")
    @MethodSource("goldenCases")
    void goldenBatches(String name, Map<String, InputValue> inputs, List<GeneOverride> overrides,
                       Optional<String> stopAfter) {
        GeneratedBatch b = generate(inputs, overrides,
                new GenerateOptions(Target.RUN, stopAfter, OptionalInt.empty(), TestFixtures.provenance()));
        assertEquals(golden(name), b.text(), name);
        assertEquals(Sha256.of(b.text()), b.sha256());
    }

    static List<GeneOverride> manyOverrides() {
        return List.of(
                new GeneOverride("hsa2354", Optional.of(Observation.NOT_DE), Optional.empty(), Optional.empty()),
                new GeneOverride("hsa3725", Optional.of(Observation.CONFIRMED_UP), Optional.of(Activity.ACTIVE),
                        Optional.empty()),
                new GeneOverride("hsa1843", Optional.empty(), Optional.of(Activity.INACTIVE), Optional.empty()),
                new GeneOverride("hsa8660", Optional.empty(), Optional.empty(), Optional.of(Relaxation.NONRELAXED)),
                new GeneOverride("hsa4609", Optional.of(Observation.CONFIRMED_DOWN), Optional.empty(), Optional.empty()),
                new GeneOverride("hsa1027", Optional.of(Observation.NOT_DE), Optional.empty(), Optional.empty()));
    }

    @Test
    void overridesBecomeOneIdPerLineFilesOnlyForKindsThatHaveGenes() {
        GeneratedBatch b = run(egfSession(), manyOverrides());
        Map<String, List<String>> files = new LinkedHashMap<>();
        b.sideFiles().forEach(f -> files.put(f.relativePath(), f.lines()));
        assertEquals(Map.of(
                "overrides/confirmed_up.txt", List.of("hsa3725"),
                "overrides/confirmed_down.txt", List.of("hsa4609"),
                "overrides/not_de.txt", List.of("hsa1027", "hsa2354"),
                "overrides/active.txt", List.of("hsa3725"),
                "overrides/inactive.txt", List.of("hsa1843"),
                "overrides/nonrelaxed.txt", List.of("hsa8660")), files);
        assertEquals("hsa1027\nhsa2354\n", b.sideFiles().stream()
                .filter(f -> f.relativePath().equals("overrides/not_de.txt")).findFirst().orElseThrow().text());
        assertTrue(run(egfSession(), List.of()).sideFiles().isEmpty());
    }

    // ---- sweeps: one run per value (10.C21), and the export's sweep line ----

    private Map<String, InputValue> sweptBound() {
        Map<String, InputValue> v = egfSession();
        v.put("bound", new InputValue.Sweep(List.of(integer(6), integer(8), integer(10))));
        return v;
    }

    @Test
    void aSweptBoundRunsOneValueAndExportsSweepLines() {
        GeneratedBatch point1 = generate(sweptBound(), List.of(),
                new GenerateOptions(Target.RUN, Optional.empty(), OptionalInt.of(1), TestFixtures.provenance()));
        assertEquals(golden("sweep-bound-run-1"), point1.text());
        GeneratedBatch export = generate(sweptBound(), List.of(),
                new GenerateOptions(Target.EXPORT, Optional.empty(), OptionalInt.empty(), TestFixtures.provenance()));
        assertEquals(golden("sweep-bound-export"), export.text());
    }

    /** The per-value batches equal what the exported sweep file runs for each value (10.C21). */
    @Test
    void perValueBatchesMatchTheExportedSweep() {
        String export = generate(sweptBound(), List.of(),
                new GenerateOptions(Target.EXPORT, Optional.empty(), OptionalInt.empty(), TestFixtures.provenance())).text();
        List<String> values = List.of("6", "8", "10");
        assertEquals(3, SweepPoints.points(standard, sweptBound()).size());
        for (int i = 0; i < values.size(); i++) {
            String run = generate(sweptBound(), List.of(),
                    new GenerateOptions(Target.RUN, Optional.empty(), OptionalInt.of(i), TestFixtures.provenance())).text();
            assertTrue(run.contains("\nlet $BOUND " + values.get(i) + "\n"));
            assertEquals(export.replace("sweep $BOUND 6 8 10 label=bound\n", ""),
                    run.replace("let $BOUND " + values.get(i) + "\n", ""));
        }
        assertThrows(PipelineException.class, () -> generate(sweptBound(), List.of(),
                GenerateOptions.run(TestFixtures.provenance())));
        assertEquals("bound-8", SweepPoints.points(standard, sweptBound()).get(1).label());
    }

    // ---- determinism ----

    /**
     * The same pipeline and session built in other orders give the same bytes: maps of arguments and
     * values are iterated in catalog and declaration order, never in insertion or hash order. FuSION
     * 1's block graph lived in identity-hashed sets ({@code gui4.java:3063-3092}).
     */
    @Test
    void shuffledMapsGiveTheSameBytes() {
        String expected = run(egfSession(), manyOverrides()).text();
        Random random = new Random(7);
        for (int round = 0; round < 20; round++) {
            List<Block> blocks = new ArrayList<>();
            for (Block b : standard.blocks()) {
                List<Map.Entry<String, Binding>> args = new ArrayList<>(b.args().entrySet());
                Collections.shuffle(args, random);
                Map<String, Binding> shuffledArgs = new LinkedHashMap<>();
                args.forEach(e -> shuffledArgs.put(e.getKey(), e.getValue()));
                blocks.add(new Block(b.id(), b.command(), shuffledArgs, b.when(), b.x(), b.y()));
            }
            PipelineDocument doc = new PipelineDocument(standard.format(), standard.origin(), standard.inputs(), blocks);
            List<Map.Entry<String, InputValue>> values = new ArrayList<>(egfSession().entrySet());
            Collections.shuffle(values, random);
            Map<String, InputValue> shuffledValues = new LinkedHashMap<>();
            values.forEach(e -> shuffledValues.put(e.getKey(), e.getValue()));
            List<GeneOverride> overrides = new ArrayList<>(manyOverrides());
            Collections.shuffle(overrides, random);
            String text = generator.generate(doc, shuffledValues, overrides, catalog,
                    GenerateOptions.run(TestFixtures.provenance())).text();
            // The header's SHA-256 is taken over canonical JSON, so the argument order cannot move it.
            assertEquals(expected, text);
        }
    }

    /** A pipeline whose connections fix the order gives the same bytes whatever the blocks' order. */
    @Test
    void shuffledBlocksOfAChainGiveTheSameBytes() {
        Map<String, Binding> corridorArgs = new LinkedHashMap<>();
        corridorArgs.put("graph", new Binding.Output("net"));
        corridorArgs.put("source", new Binding.Literal(text("hsa1")));
        corridorArgs.put("target", new Binding.Literal(text("hsa22")));
        corridorArgs.put("bound", new Binding.Literal(integer(4)));
        List<Block> chain = List.of(
                new Block("net", "rgx", Map.of("file", new Binding.Literal(path("/data/net.xml"))), List.of(), 0, 0),
                new Block("cor", "fb_rch", corridorArgs, List.of(), 0, 0),
                new Block("sol", "pathz3", Map.of("graph", new Binding.Output("cor")), List.of(), 0, 0),
                new Block("plot", "plot_pareto", Map.of("solve", new Binding.Output("sol")), List.of(), 0, 0));
        String expected = null;
        Random random = new Random(11);
        for (int round = 0; round < 10; round++) {
            List<Block> blocks = new ArrayList<>(chain);
            Collections.shuffle(blocks, random);
            PipelineDocument doc = new PipelineDocument(1, "test@1", List.of(), blocks);
            String text = generator.generate(doc, Map.of(), List.of(), catalog,
                    GenerateOptions.run(TestFixtures.provenance())).text();
            String body = text.substring(text.indexOf('\n', text.indexOf('\n') + 1) + 1);
            if (expected == null) {
                expected = body;
            }
            assertEquals(expected, body);
        }
        assertEquals("rgx file=/data/net.xml\nfb_rch source=hsa1 target=hsa22 bound=4\npathz3\nplot_pareto\nexit\n", expected);
    }

    @Test
    void everyUsedInputHasExactlyOneLetAndTheSessionNameNeverAppears() {
        GeneratedBatch b = run(egfSession(), manyOverrides());
        List<String> lets = b.text().lines().filter(l -> l.startsWith("let ")).toList();
        assertEquals(lets.size(), lets.stream().map(l -> l.split(" ")[1]).distinct().count());
        for (String l : lets) {
            String variable = l.split(" ")[1];
            assertTrue(b.text().contains("=" + variable + " ") || b.text().contains("=" + variable + "\n"),
                    variable + " is used");
        }
        assertFalse(b.text().contains("EGFR to CXCL8"));
        assertTrue(b.text().contains("session 0b6f2c1e-6a4d-4f0e-9d1a-5c3e8f2b7a90"));
    }

    @Test
    void theSourceMapNamesBlocksAndInputs() {
        GeneratedBatch b = run(egfSession(), TestFixtures.fosbNotDe());
        List<String> lines = b.text().lines().toList();
        int cudf = lines.indexOf("cudf values=$LFC up=$UP down=$DOWN not_de=$FUSION_BATCH_DIR/overrides/not_de.txt") + 1;
        assertEquals(Optional.of("classify"), b.sourceMap().block(cudf));
        assertEquals(Optional.of("up"), b.sourceMap().input(cudf, "up"));
        assertEquals(Optional.of("lfc"), b.sourceMap().input(cudf, "values"));
        assertEquals(Optional.empty(), b.sourceMap().input(cudf, "not_de"));
        int letUp = lines.indexOf("let $UP 1.01") + 1;
        assertEquals(Optional.of("up"), b.sourceMap().input(letUp, ""));
    }

    // ---- values and quoting (01.C2) ----

    @Test
    void quotingTable() {
        assertEquals("plain", BatchText.token("plain"));
        assertEquals("\"two words\"", BatchText.token("two words"));
        assertEquals("\"a\tb\"", BatchText.token("a\tb"));
        assertEquals("\"say \"\"hi\"\"\"", BatchText.token("say \"hi\""));
        assertEquals("$$HOME/x", BatchText.token("$HOME/x"));
        assertEquals("\"$$HOME/my x\"", BatchText.token("$HOME/my x"));
        assertEquals("C:\\Users\\José\\FuSION\\egf.tsv", BatchText.token("C:\\Users\\José\\FuSION\\egf.tsv"));
        assertEquals("\"\"", BatchText.token(""));
        assertEquals("a=b", BatchText.token("a=b"));
        assertThrows(PipelineException.class, () -> BatchText.token("two\nlines"));
        assertThrows(PipelineException.class, () -> BatchText.token("bell\u0007"));
        assertThrows(PipelineException.class, () -> BatchText.token("line\u2028separator"));
        assertEquals("$FUSION_HOME/data/kegg_pathways.txt",
                BatchText.value(new InputValue.InstallPath("data/kegg_pathways.txt")));
        assertEquals("\"$FUSION_HOME/data/my list.txt\"",
                BatchText.value(new InputValue.InstallPath("data/my list.txt")));
        assertEquals("$FUSION_HOME/data/a$$b.txt", BatchText.value(new InputValue.InstallPath("data\\a$b.txt")));
        assertThrows(PipelineException.class, () -> BatchText.value(new InputValue.PathValue(Path.of("relative/x.tsv"))));
    }

    @Test
    void listValuesAreJoinedWithCommasAndAnItemWithACommaIsRefused() {
        assertEquals("hsa1,hsa2,hsa3", BatchText.value(TestFixtures.genes("hsa1", "hsa2", "hsa3")));
        assertEquals("\"a b\",c", BatchText.value(new InputValue.Items(List.of(text("a b"), text("c")))));
        assertThrows(PipelineException.class,
                () -> BatchText.value(new InputValue.Items(List.of(text("a,b")))));
        Map<String, InputValue> v = egfSession();
        v.put("fsc", TestFixtures.genes("hsa3725", "hsa1843"));
        assertTrue(run(v, List.of()).text().contains("\nlet $FSC hsa3725,hsa1843\n"));
    }

    @Test
    void realsAreWrittenTheSameUnderEveryLocale() {
        Locale before = Locale.getDefault();
        try {
            for (Locale l : List.of(Locale.GERMANY, Locale.FRANCE, Locale.ROOT)) {
                Locale.setDefault(l);
                assertEquals("1.01", BatchText.value(TestFixtures.real(1.01)));
                assertEquals("-1.01", BatchText.value(TestFixtures.real(-1.01)));
                assertEquals("0.0000001", BatchText.value(TestFixtures.real(1e-7)));
                assertEquals("700", BatchText.value(TestFixtures.real(700)));
                assertEquals("0", BatchText.value(TestFixtures.real(-0.0)));
                assertEquals(golden("kgml-gsea"), run(egfSession(), TestFixtures.fosbNotDe()).text());
            }
        } finally {
            Locale.setDefault(before);
        }
    }

    // ---- refusals: an unfilled value stops generation (S8.17, S8.20) ----

    @Test
    void anUnfilledRequiredValueStopsGeneration() {
        Map<String, InputValue> v = egfSession();
        v.put("up", InputValue.UNSET);
        PipelineException e = assertThrows(PipelineException.class, () -> run(v, List.of()));
        assertEquals("generate.invalid", e.code());
        assertTrue(e.getMessage().contains("Up threshold is not set"), e.getMessage());
        Map<String, InputValue> w = egfSession();
        w.put("lfc", text("bad\nvalue"));
        assertThrows(PipelineException.class, () -> run(w, List.of()));
    }

    @Test
    void stopAfterAnUnknownOrDisabledBlockIsRefused() {
        Map<String, InputValue> v = egfSession();
        assertThrows(PipelineException.class, () -> generate(v, List.of(),
                new GenerateOptions(Target.RUN, Optional.of("merged_network"), OptionalInt.empty(), TestFixtures.provenance())));
        // Stopping after the corridor keeps what it depends on: the network and the classification.
        String text = generate(v, List.of(), new GenerateOptions(Target.RUN, Optional.of("corridor"),
                OptionalInt.empty(), TestFixtures.provenance())).text();
        List<String> commands = text.lines().filter(l -> !l.startsWith("let ") && !l.startsWith(";;"))
                .map(l -> l.split(" ")[0]).toList();
        assertEquals(List.of("cudf", "gsea", "mff", "fb_rch", "exit"), commands);
    }

    @Test
    void aSavedBindingIsWrittenAsALoadLine() {
        Map<String, Binding> args = new LinkedHashMap<>();
        args.put("graph", new Binding.Saved(Path.of("/runs/egfr_20261112-181406"), "fb_rch"));
        args.put("fsc", new Binding.Literal(TestFixtures.genes("hsa6774")));
        args.put("points", new Binding.Literal(text("7,0")));
        PipelineDocument doc = new PipelineDocument(1, "resolve@1", List.of(),
                List.of(new Block("solve", "pathz3", args, List.of(), 0, 0)));
        String text = generator.generate(doc, Map.of(), List.of(), catalog,
                GenerateOptions.run(TestFixtures.provenance())).text();
        assertTrue(text.endsWith("load run=/runs/egfr_20261112-181406 step=fb_rch\npathz3 fsc=hsa6774 points=7,0\nexit\n"),
                text);
    }
}
