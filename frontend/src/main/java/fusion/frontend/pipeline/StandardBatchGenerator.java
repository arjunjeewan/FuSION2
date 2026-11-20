package fusion.frontend.pipeline;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Section 10's batch generator (10.C14, S8.18 to S8.20): a pure function from a pipeline, a
 * session's values and overrides, and the catalog to a batch in section 01's language. It replaces
 * FuSION 1's template filler, which substituted placeholders by plain text into a template read from
 * the launch directory, left three placeholders unfilled and wrote overrides as gene text
 * ({@code Linux/frontend/src/ScriptFile.java:8-84}; S8.15, S8.17, S8.20).
 *
 * <p>The batch holds only the arguments the pipeline binds, so {@code fusion} applies and records
 * its own defaults (10.C14). Its steps: resolve, trim, order, write, values, side files, encoding.
 */
public final class StandardBatchGenerator implements BatchGenerator {
    @Override
    public GeneratedBatch generate(PipelineDocument pipeline, Map<String, InputValue> inputs,
                                   List<GeneOverride> overrides, Catalog catalog, GenerateOptions options) {
        // 1. Resolve, with one swept point fixed for a run (10.C21).
        Map<String, InputValue> values = inputs;
        List<String> swept = SweepPoints.sweptInputs(pipeline, inputs);
        if (options.target() == Target.RUN && !swept.isEmpty()) {
            List<SweepPoints.Point> points = SweepPoints.points(pipeline, inputs);
            int index = options.sweepValue().orElseThrow(() -> new PipelineException("generate.sweep",
                    "a swept session is run one point at a time; no point was given"));
            if (index < 0 || index >= points.size()) {
                throw new PipelineException("generate.sweep", "the sweep has no point " + index);
            }
            values = SweepPoints.fix(inputs, points.get(index));
            swept = List.of();
        }
        Map<String, List<String>> overrideIds = GeneOverride.idsByKind(overrides);
        PipelinePlan plan = PipelinePlan.of(pipeline, values, overrideIds.keySet(), catalog);
        if (plan.hasErrors()) {
            PipelineProblem first = plan.problems.stream().filter(PipelineProblem::isError).findFirst().orElseThrow();
            throw new PipelineException("generate.invalid", "the pipeline cannot be written as a batch: "
                    + first.message() + " [" + first.code() + "]");
        }

        // 2. Trim, for stop after a step (decision 0019; written as an exit after it, 01.C6).
        List<PipelinePlan.Planned> blocks = plan.order;
        if (options.stopAfter().isPresent()) {
            String stop = options.stopAfter().get();
            if (!plan.planned.containsKey(stop)) {
                throw new PipelineException("generate.stop_after", "block " + stop + " does not run in this session");
            }
            Set<String> keep = plan.closure(stop);
            blocks = blocks.stream().filter(p -> keep.contains(p.block().id())).toList();
        }
        // 3. Order: the plan's topological order, ties by the document.
        Set<String> used = new HashSet<>();
        for (PipelinePlan.Planned p : blocks) {
            for (Binding b : p.args().values()) {
                if (b instanceof Binding.Input in) {
                    used.add(in.name());
                }
            }
        }

        // 4. Write.
        List<String> lines = new ArrayList<>();
        Map<Integer, String> blockByLine = new LinkedHashMap<>();
        Map<String, String> inputByLine = new LinkedHashMap<>();
        Provenance prov = options.provenance();
        lines.add(";; FuSION 2.0 batch written by the FuSION frontend " + oneLine(prov.frontendVersion()));
        lines.add(";; session " + oneLine(prov.sessionId()) + ", pipeline " + oneLine(pipeline.origin())
                + " sha256 " + PipelineJson.sha256(pipeline));
        if (options.target() == Target.EXPORT) {
            for (String name : swept) {
                if (!used.contains(name)) {
                    continue;
                }
                InputDecl d = pipeline.input(name).orElseThrow();
                List<String> tokens = new ArrayList<>();
                for (InputValue v : ((InputValue.Sweep) plan.value(name)).values()) {
                    tokens.add(BatchText.value(v));
                }
                // Section 01's sweep syntax (01.C3); the swept input gets no let (S1.08).
                lines.add("sweep " + d.variable() + " " + String.join(" ", tokens) + " label=" + name);
                inputByLine.put(SourceMap.key(lines.size(), ""), name);
            }
        }
        for (InputDecl d : pipeline.inputs()) {
            if (!used.contains(d.name()) || (options.target() == Target.EXPORT && swept.contains(d.name()))) {
                continue;
            }
            lines.add("let " + d.variable() + " " + BatchText.value(plan.value(d.name())));
            inputByLine.put(SourceMap.key(lines.size(), ""), d.name());
        }
        Set<String> loaded = new HashSet<>();
        for (PipelinePlan.Planned p : blocks) {
            String id = p.block().id();
            for (Binding b : p.args().values()) {
                if (b instanceof Binding.Saved s && loaded.add(s.runDir() + "\n" + s.step())) {
                    lines.add("load run=" + BatchText.value(new InputValue.PathValue(s.runDir()))
                            + " step=" + BatchText.token(s.step()));
                    blockByLine.put(lines.size(), id);
                }
            }
            StringBuilder line = new StringBuilder(p.command().name());
            List<String[]> argInputs = new ArrayList<>();
            for (Map.Entry<String, Binding> e : p.args().entrySet()) {
                String text = switch (e.getValue()) {
                    case Binding.Literal l -> BatchText.value(l.value());
                    case Binding.Input in -> {
                        argInputs.add(new String[] {e.getKey(), in.name()});
                        yield pipeline.input(in.name()).orElseThrow().variable();
                    }
                    case Binding.Overrides o -> "$FUSION_BATCH_DIR/overrides/" + o.kind() + ".txt";
                    // A connection or a saved step is the latest value of its kind (01.C16).
                    case Binding.Output out -> null;
                    case Binding.Saved s -> null;
                    case Binding.FirstOf f -> throw new IllegalStateException("first_of is resolved");
                };
                if (text != null) {
                    line.append(' ').append(e.getKey()).append('=').append(text);
                }
            }
            lines.add(line.toString());
            blockByLine.put(lines.size(), id);
            for (String[] ai : argInputs) {
                inputByLine.put(SourceMap.key(lines.size(), ai[0]), ai[1]);
            }
        }
        lines.add("exit");

        // 6. Side files: one id per line, only for kinds that have genes (S8.15).
        List<SideFile> sideFiles = new ArrayList<>();
        for (String kind : GeneOverride.KINDS) {
            List<String> ids = overrideIds.get(kind);
            if (ids != null) {
                sideFiles.add(new SideFile("overrides/" + kind + ".txt", ids));
            }
        }
        // 7. Encoding: \n line ends, UTF-8 without a byte-order mark (01.C19).
        String text = String.join("\n", lines) + "\n";
        return new GeneratedBatch(text, Sha256.of(text), sideFiles, new SourceMap(blockByLine, inputByLine));
    }

    private static String oneLine(String text) {
        BatchText.refuseControl(text);
        return text;
    }
}
