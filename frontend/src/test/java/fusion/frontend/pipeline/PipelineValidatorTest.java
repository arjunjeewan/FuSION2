package fusion.frontend.pipeline;

import static fusion.frontend.pipeline.TestFixtures.egfSession;
import static fusion.frontend.pipeline.TestFixtures.integer;
import static fusion.frontend.pipeline.TestFixtures.path;
import static fusion.frontend.pipeline.TestFixtures.real;
import static fusion.frontend.pipeline.TestFixtures.text;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.OptionalDouble;

import org.junit.jupiter.api.Test;

/** Section 10's test hooks for {@code PipelineValidator}: validation steps 1 and 2 (S8.30). */
class PipelineValidatorTest {
    private final PipelineValidator validator = new StandardPipelineValidator();
    private final PipelineDocument standard = TestFixtures.standard();
    private final Catalog catalog = TestFixtures.catalog();

    private List<PipelineProblem> errors(PipelineDocument doc, Map<String, InputValue> inputs) {
        return validator.validate(doc, inputs, catalog).stream().filter(PipelineProblem::isError).toList();
    }

    private static List<String> codes(List<PipelineProblem> problems) {
        return problems.stream().map(PipelineProblem::code).toList();
    }

    @Test
    void theReferenceSessionHasNoProblem() {
        assertEquals(List.of(), validator.validate(standard, egfSession(), catalog));
        assertEquals(List.of(), validator.validate(standard, TestFixtures.toyPremergedSession(), catalog));
    }

    @Test
    void requiredInputsAndTheirMessages() {
        Map<String, InputValue> v = egfSession();
        v.put("up", InputValue.UNSET);
        v.put("timeout", InputValue.UNSET);
        v.put("source", new InputValue.GeneId(""));
        List<PipelineProblem> e = errors(standard, v);
        assertEquals(List.of("input.required", "input.required", "input.required"), codes(e));
        assertEquals(Optional.of("up"), e.get(0).input());
        assertEquals("Up threshold is required", e.get(0).message());
        // The fresh session of a new user: no thresholds, bounds or timeout (S8.06, 10.C13).
        Map<String, InputValue> fresh = new LinkedHashMap<>();
        standard.inputs().forEach(d -> fresh.put(d.name(), d.initialValue()));
        assertEquals(List.of("lfc", "up", "down", "source", "target", "node_max", "edge_max", "timeout"),
                errors(standard, fresh).stream().map(p -> p.input().orElseThrow()).toList());
    }

    @Test
    void typesRangesChoicesAndSyntax() {
        Map<String, InputValue> v = egfSession();
        v.put("bound", integer(0));                       // S8.09: a bound below 1 is refused
        v.put("node_max", integer(101));                  // S8.11: 0 to 100
        v.put("timeout", text("4 hours"));                // a duration needs a unit (01.C1)
        v.put("edges_to_target", TestFixtures.choice("activation-only"));
        v.put("lfc", path("relative/egf.tsv"));           // batches hold absolute paths (01.C2)
        v.put("target", new InputValue.GeneId("hsa3576 - cxcl8"));   // S8.12: bare ids only
        v.put("up", text("1,01"));
        List<PipelineProblem> e = errors(standard, v);
        assertEquals(List.of("input.relative_path", "input.bad_type", "input.bad_gene_id", "input.below_min",
                "input.bad_choice", "input.above_max", "input.bad_type"), codes(e));
    }

    @Test
    void theCatalogsRulesAreEvaluatedAsTheBackendWould() {
        Map<String, InputValue> v = egfSession();
        v.put("up", real(-1.5));                          // S3.07: up must lie above down
        v.put("node_min", integer(9));                    // S8.11: lower <= upper
        List<PipelineProblem> e = errors(standard, v);
        assertEquals(List.of("rule.less", "rule.less_or_equal"), codes(e));
        assertEquals(Optional.of("down"), e.get(0).input());
        assertTrue(e.get(0).message().startsWith("Down threshold (-1.01) must be below Up threshold (-1.5)"),
                e.get(0).message());
        Map<String, InputValue> w = egfSession();
        w.put("coexpression_csv", InputValue.UNSET);
        assertEquals(List.of("rule.requires"), codes(errors(standard, w)));
        Map<String, InputValue> equal = egfSession();
        equal.put("up", real(1));
        equal.put("down", real(1));
        assertEquals(List.of("rule.less"), codes(errors(standard, equal)));
    }

    @Test
    void theCatalogsOwnRangesApplyToBoundValues() {
        Catalog strict = new Catalog(1, "0", catalog.commands().stream().map(c -> c.name().equals("fb_rch")
                ? new CommandSpec(c.name(), c.aliases(), c.stage(), c.summary(), c.help(),
                        c.args().stream().map(a -> a.name().equals("bound")
                                ? new ArgSpec(a.name(), a.setting(), a.type(), a.list(), a.takes(), a.need(),
                                        a.defaultValue(), a.min(), OptionalDouble.of(20), a.choices(), a.debug(), a.help())
                                : a).toList(), c.rules(), c.needs(), c.produces(), c.debug())
                : c).toList(), catalog.runSettings());
        Map<String, InputValue> v = egfSession();
        v.put("bound", integer(25));
        List<PipelineProblem> e = validator.validate(standard, v, strict).stream().filter(PipelineProblem::isError).toList();
        assertEquals(List.of("argument.above_max"), codes(e));
        assertEquals(Optional.of("corridor"), e.get(0).block());
    }

    @Test
    void requiredArgumentsLeftUnboundAfterConditionsAndFirstOf() {
        // The pre-merged source needs its file: rgx's required file is then left unbound.
        Map<String, InputValue> v = TestFixtures.toyPremergedSession();
        v.put("merged_graph", InputValue.UNSET);
        List<PipelineProblem> e = errors(standard, v);
        assertEquals(List.of("pipeline.unbound_required"), codes(e));
        assertEquals(Optional.of("merged_network"), e.get(0).block());
        assertEquals(Optional.of("file"), e.get(0).argument());
        assertEquals(Optional.of("merged_graph"), e.get(0).input());
        // gsea is required by the backend's table; with the source a pathway list, its values must be set.
        Map<String, InputValue> w = egfSession();
        w.put("gsea_selector", InputValue.UNSET);
        e = errors(standard, w);
        assertEquals(List.of("pipeline.unbound_required"), codes(e));
        assertEquals(Optional.of("gsea"), e.get(0).block());
        assertEquals(Optional.of("selector"), e.get(0).argument());
        // ... and once GSEA is off, its block is disabled, so nothing is missing.
        w.put("use_gsea", new InputValue.Flag(false));
        assertEquals(List.of(), errors(standard, w));
    }

    private static Block block(String id, String command, Map<String, Binding> args) {
        return new Block(id, command, args, List.of(), 0, 0);
    }

    private static Map<String, Binding> corridorArgs(Binding graph) {
        Map<String, Binding> a = new LinkedHashMap<>();
        if (graph != null) {
            a.put("graph", graph);
        }
        a.put("source", new Binding.Literal(text("hsa1")));
        a.put("target", new Binding.Literal(text("hsa22")));
        a.put("bound", new Binding.Literal(integer(4)));
        return a;
    }

    /** FuSION 1's cycle check could never fire ({@code gui4.java:3094-3102}); here each cycle is named. */
    @Test
    void cyclesAreFoundAndEachComponentIsNamed() {
        PipelineDocument doc = new PipelineDocument(1, "test@1", List.of(), List.of(
                block("a", "fb_rch", corridorArgs(new Binding.Output("c"))),
                block("b", "pathz3", Map.of("graph", new Binding.Output("a"))),
                block("c", "rgx", Map.of("file", new Binding.Literal(path("/x.xml")),
                        "split_threshold", new Binding.FirstOf(List.of(new Binding.Output("b"))))),
                block("d", "fb_rch", corridorArgs(new Binding.Output("d"))),
                block("e", "rgx", Map.of("file", new Binding.Literal(path("/y.xml"))))));
        List<PipelineProblem> e = errors(doc, Map.of());
        assertEquals(List.of("pipeline.cycle", "pipeline.cycle"), codes(e));
        List<String> messages = e.stream().map(PipelineProblem::message).sorted().toList();
        assertEquals(List.of("these blocks depend on each other in a cycle: a, b, c",
                "these blocks depend on each other in a cycle: d"), messages);
    }

    @Test
    void kindMismatchesAndConnectionsThatAreNotTheLatest() {
        PipelineDocument mismatch = new PipelineDocument(1, "test@1", List.of(), List.of(
                block("net", "rgx", Map.of("file", new Binding.Literal(path("/x.xml")))),
                block("cor", "fb_rch", corridorArgs(new Binding.Output("net"))),
                block("sol", "pathz3", Map.of("graph", new Binding.Output("net")))));
        assertEquals(List.of("pipeline.kind_mismatch"), codes(errors(mismatch, Map.of())));

        PipelineDocument stale = new PipelineDocument(1, "test@1", List.of(), List.of(
                block("first", "rgx", Map.of("file", new Binding.Literal(path("/x.xml")))),
                block("second", "rgx", Map.of("file", new Binding.Literal(path("/y.xml")))),
                block("cor", "fb_rch", corridorArgs(new Binding.Output("first")))));
        List<PipelineProblem> e = errors(stale, Map.of());
        assertEquals(List.of("pipeline.not_latest"), codes(e));
        assertTrue(e.get(0).message().contains("which block second makes, not block first"), e.get(0).message());
    }

    @Test
    void unknownCommandsArgumentsInputsAndDebugOptions() {
        PipelineDocument doc = new PipelineDocument(1, "test@1", List.of(), List.of(
                block("x", "upfx", Map.of()),
                block("y", "fb_rch", Map.of("bogus", new Binding.Literal(text("1")))),
                block("z", "rgx", Map.of("file", new Binding.Input("nowhere")))));
        assertEquals(List.of("pipeline.unknown_command", "pipeline.unknown_argument", "pipeline.unknown_input"),
                codes(errors(doc, Map.of())));

        Catalog withDebug = new Catalog(1, "0", List.of(new CommandSpec("rgx", List.of(), "", "", "",
                List.of(new ArgSpec("file", "network.file", ArgType.INPUT_FILE, false, Optional.empty(), Need.REQUIRED,
                                Optional.empty(), OptionalDouble.empty(), OptionalDouble.empty(), List.of(), false, ""),
                        new ArgSpec("trace", "network.trace", ArgType.BOOLEAN, false, Optional.empty(), Need.OPTIONAL,
                                Optional.empty(), OptionalDouble.empty(), OptionalDouble.empty(), List.of(), true, "")),
                List.of(), List.of(), Optional.of(ValueKind.NETWORK), false)), List.of());
        PipelineDocument debug = new PipelineDocument(1, "test@1", List.of(), List.of(block("r", "rgx",
                Map.of("file", new Binding.Literal(path("/x.xml")), "trace", new Binding.Literal(new InputValue.Flag(true))))));
        // S1.29: the frontend never sets a debug option.
        assertEquals(List.of("pipeline.debug_argument"), codes(validator.validate(debug, Map.of(), withDebug)));
    }

    @Test
    void aStepWhoseValueNoBlockMakesIsRefused() {
        PipelineDocument doc = new PipelineDocument(1, "test@1", List.of(), List.of(
                block("cor", "fb_rch", corridorArgs(null)),
                block("net", "rgx", Map.of("file", new Binding.Literal(path("/x.xml"))))));
        List<PipelineProblem> e = errors(doc, Map.of());
        assertEquals(List.of("pipeline.missing_value"), codes(e));
        assertEquals(Optional.of("cor"), e.get(0).block());
    }

    @Test
    void inputsTheDefaultPipelineNoLongerDeclaresAreKeptAndFlagged() {
        Map<String, InputValue> v = egfSession();
        v.put("relaxed_edges", path("/old/relaxed.txt"));   // dropped in entry 165
        List<PipelineProblem> all = validator.validate(standard, v, catalog);
        assertEquals(List.of("input.unused"), codes(all));
        assertEquals(Severity.WARNING, all.get(0).severity());
    }

    @Test
    void badInputNamesAndSweepsOfInputsThatCannotBeSwept() {
        InputDecl reserved = new InputDecl("fusion_home", ArgType.TEXT, false, Optional.empty(), false,
                Optional.empty(), Optional.empty(), OptionalDouble.empty(), OptionalDouble.empty(), List.of(),
                false, false, "x", "", "");
        PipelineDocument doc = new PipelineDocument(1, "t@1", List.of(reserved), List.of());
        assertEquals(List.of("pipeline.bad_input_name"), codes(errors(doc, Map.of())));
        Map<String, InputValue> v = egfSession();
        v.put("up", new InputValue.Sweep(List.of(real(1), real(2))));
        assertEquals(List.of("input.not_sweepable"), codes(errors(standard, v)));
    }
}
