package fusion.frontend.pipeline;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Deque;
import java.util.EnumMap;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.PriorityQueue;
import java.util.Set;
import java.util.TreeSet;
import java.util.regex.Pattern;

/**
 * A pipeline resolved against a session's values and the catalog: the structural problems, the
 * enabled blocks in their order, and each block's resolved bindings and dependencies. The validator
 * reports its problems (S8.30); the generator writes its order (10.C14).
 *
 * <p>The order is topological, with ties broken by the blocks' order in the document, so one
 * pipeline always gives one order. FuSION 1 kept its block graph in {@code HashSet}s of blocks
 * with identity hash codes, so independent blocks could change order between two runs of the GUI,
 * and its cycle check could never fire, because the ordering added any unvisited block whenever its
 * queue emptied ({@code Linux/frontend/src/gui4.java:3062-3128}; S8.18).
 */
final class PipelinePlan {
    /** An enabled block with its command, its resolved bindings in catalog order and what it depends on. */
    record Planned(Block block, int index, CommandSpec command, Map<String, Binding> args, Set<String> dependsOn) {}

    static final Pattern INPUT_NAME = Pattern.compile("[a-z][a-z0-9_]*");

    final PipelineDocument doc;
    final Catalog catalog;
    /** The effective value of every declared input: the session's, or the declaration's default. */
    final Map<String, InputValue> values = new LinkedHashMap<>();
    /** Session values for inputs the pipeline does not declare (10.C8: kept, flagged unused). */
    final Map<String, InputValue> undeclared = new LinkedHashMap<>();
    final List<PipelineProblem> problems = new ArrayList<>();
    final List<Planned> order = new ArrayList<>();
    final Map<String, Planned> planned = new HashMap<>();
    /** Blocks whose conditions hold. */
    final Set<String> enabled = new LinkedHashSet<>();

    private final Set<String> overrideKinds;          // kinds with genes; empty when not known
    private final Map<String, Integer> blockIndex = new HashMap<>();
    private final Map<String, CommandSpec> commands = new HashMap<>();

    private PipelinePlan(PipelineDocument doc, Catalog catalog, Set<String> overrideKinds) {
        this.doc = doc;
        this.catalog = catalog;
        this.overrideKinds = overrideKinds;
    }

    static PipelinePlan of(PipelineDocument doc, Map<String, InputValue> inputs, Set<String> overrideKinds,
                           Catalog catalog) {
        PipelinePlan plan = new PipelinePlan(doc, catalog, Set.copyOf(overrideKinds));
        plan.collectValues(inputs);
        boolean sound = plan.checkStructure();
        if (sound && plan.checkCycles()) {
            plan.resolveAndOrder();
        }
        return plan;
    }

    boolean hasErrors() {
        return problems.stream().anyMatch(PipelineProblem::isError);
    }

    InputValue value(String input) {
        return values.getOrDefault(input, InputValue.UNSET);
    }

    // ---- values ---------------------------------------------------------------------------------

    private void collectValues(Map<String, InputValue> inputs) {
        for (InputDecl d : doc.inputs()) {
            values.put(d.name(), inputs.containsKey(d.name()) ? inputs.get(d.name()) : d.initialValue());
        }
        for (Map.Entry<String, InputValue> e : new java.util.TreeMap<>(inputs).entrySet()) {
            if (doc.input(e.getKey()).isEmpty()) {
                undeclared.put(e.getKey(), e.getValue());
            }
        }
    }

    // ---- structure ------------------------------------------------------------------------------

    private boolean checkStructure() {
        Set<String> inputNames = new HashSet<>();
        for (InputDecl d : doc.inputs()) {
            if (!INPUT_NAME.matcher(d.name()).matches() || d.name().startsWith("fusion_")) {
                // $FUSION_ is reserved for section 01's predefined variables (01.C18).
                problems.add(PipelineProblem.forInput(Severity.ERROR, d.name(), "pipeline.bad_input_name",
                        "input name '" + d.name() + "' must be lower-case letters, digits and _, and not start with fusion_"));
            }
            if (!inputNames.add(d.name())) {
                problems.add(PipelineProblem.forInput(Severity.ERROR, d.name(), "pipeline.duplicate_input",
                        "input '" + d.name() + "' is declared twice"));
            }
        }
        boolean sound = true;
        for (int i = 0; i < doc.blocks().size(); i++) {
            Block b = doc.blocks().get(i);
            if (b.id().isBlank() || blockIndex.putIfAbsent(b.id(), i) != null) {
                problems.add(PipelineProblem.forBlock(Severity.ERROR, b.id(), "pipeline.duplicate_block",
                        "block id '" + b.id() + "' is empty or used twice"));
                sound = false;
            }
        }
        for (Block b : doc.blocks()) {
            Optional<CommandSpec> spec = catalog.command(b.command());
            if (spec.isEmpty()) {
                problems.add(PipelineProblem.forBlock(Severity.ERROR, b.id(), "pipeline.unknown_command",
                        "block " + b.id() + ": fusion has no command '" + b.command() + "'"));
                sound = false;
                continue;
            }
            CommandSpec c = spec.get();
            commands.put(b.id(), c);
            if (c.debug()) {
                // S1.29: the frontend never shows or sets a debug option.
                problems.add(PipelineProblem.forBlock(Severity.ERROR, b.id(), "pipeline.debug_command",
                        "block " + b.id() + ": " + c.name() + " is a debug command"));
            }
            for (Map.Entry<String, Binding> e : b.args().entrySet()) {
                Optional<ArgSpec> arg = c.arg(e.getKey());
                if (arg.isEmpty()) {
                    problems.add(PipelineProblem.forArgument(Severity.ERROR, b.id(), e.getKey(), Optional.empty(),
                            "pipeline.unknown_argument", "block " + b.id() + ": " + c.name()
                                    + " has no argument '" + e.getKey() + "'; it takes "
                                    + String.join(", ", c.args().stream().map(ArgSpec::name).toList())));
                    sound = false;
                } else if (arg.get().debug()) {
                    problems.add(PipelineProblem.forArgument(Severity.ERROR, b.id(), e.getKey(), Optional.empty(),
                            "pipeline.debug_argument", "block " + b.id() + ": " + e.getKey() + " is a debug argument"));
                }
                if (!checkReferences(b, e.getKey(), e.getValue())) {
                    sound = false;
                }
            }
            for (Condition cond : b.when()) {
                if (doc.input(cond.input()).isEmpty()) {
                    problems.add(PipelineProblem.forBlock(Severity.ERROR, b.id(), "pipeline.unknown_input",
                            "block " + b.id() + ": its condition names undeclared input '" + cond.input() + "'"));
                    sound = false;
                }
            }
        }
        return sound;
    }

    private boolean checkReferences(Block b, String arg, Binding binding) {
        return switch (binding) {
            case Binding.Input in -> {
                if (doc.input(in.name()).isPresent()) {
                    yield true;
                }
                problems.add(PipelineProblem.forArgument(Severity.ERROR, b.id(), arg, Optional.empty(),
                        "pipeline.unknown_input", "block " + b.id() + ": " + arg + " names undeclared input '"
                                + in.name() + "'"));
                yield false;
            }
            case Binding.Output out -> {
                if (blockIndex.containsKey(out.block())) {
                    yield true;
                }
                problems.add(PipelineProblem.forArgument(Severity.ERROR, b.id(), arg, Optional.empty(),
                        "pipeline.unknown_block", "block " + b.id() + ": " + arg + " connects to unknown block '"
                                + out.block() + "'"));
                yield false;
            }
            case Binding.FirstOf f -> {
                boolean ok = true;
                for (Binding option : f.options()) {
                    ok &= checkReferences(b, arg, option);
                }
                yield ok;
            }
            case Binding.Literal l -> true;
            case Binding.Saved s -> true;
            case Binding.Overrides o -> true;
        };
    }

    // ---- cycles ---------------------------------------------------------------------------------

    /** Every connection drawn in the document, enabled or not: producer to consumers. */
    private Map<String, Set<String>> drawnEdges() {
        Map<String, Set<String>> edges = new LinkedHashMap<>();
        for (Block b : doc.blocks()) {
            edges.putIfAbsent(b.id(), new TreeSet<>());
        }
        for (Block b : doc.blocks()) {
            for (Binding binding : b.args().values()) {
                for (String producer : outputsIn(binding)) {
                    edges.get(producer).add(b.id());
                }
            }
        }
        return edges;
    }

    private static List<String> outputsIn(Binding binding) {
        List<String> out = new ArrayList<>();
        if (binding instanceof Binding.Output o) {
            out.add(o.block());
        } else if (binding instanceof Binding.FirstOf f) {
            f.options().forEach(option -> out.addAll(outputsIn(option)));
        }
        return out;
    }

    /**
     * Kahn's algorithm finds whether a cycle exists; Tarjan's algorithm then gives the strongly
     * connected components, and each component that holds a cycle is named. Both are O(B + C).
     */
    private boolean checkCycles() {
        Map<String, Set<String>> edges = drawnEdges();
        Map<String, Integer> indegree = new HashMap<>();
        edges.keySet().forEach(id -> indegree.put(id, 0));
        edges.values().forEach(targets -> targets.forEach(t -> indegree.merge(t, 1, Integer::sum)));
        Deque<String> ready = new ArrayDeque<>();
        indegree.forEach((id, d) -> {
            if (d == 0) {
                ready.add(id);
            }
        });
        int seen = 0;
        while (!ready.isEmpty()) {
            String id = ready.poll();
            seen++;
            for (String t : edges.get(id)) {
                if (indegree.merge(t, -1, Integer::sum) == 0) {
                    ready.add(t);
                }
            }
        }
        if (seen == edges.size()) {
            return true;
        }
        for (List<String> component : new Tarjan(edges).components()) {
            boolean selfLoop = component.size() == 1 && edges.get(component.get(0)).contains(component.get(0));
            if (component.size() > 1 || selfLoop) {
                component.sort((a, b) -> Integer.compare(blockIndex.get(a), blockIndex.get(b)));
                problems.add(PipelineProblem.forBlock(Severity.ERROR, component.get(0), "pipeline.cycle",
                        "these blocks depend on each other in a cycle: " + String.join(", ", component)));
            }
        }
        return false;
    }

    /** Tarjan's strongly connected components, iterative, over the drawn connections. */
    private static final class Tarjan {
        private final Map<String, Set<String>> edges;
        private final Map<String, Integer> index = new HashMap<>();
        private final Map<String, Integer> low = new HashMap<>();
        private final Deque<String> stack = new ArrayDeque<>();
        private final Set<String> onStack = new HashSet<>();
        private final List<List<String>> components = new ArrayList<>();
        private int counter;

        Tarjan(Map<String, Set<String>> edges) {
            this.edges = edges;
        }

        List<List<String>> components() {
            for (String v : edges.keySet()) {
                if (!index.containsKey(v)) {
                    visit(v);
                }
            }
            return components;
        }

        private void visit(String root) {
            Deque<Map.Entry<String, java.util.Iterator<String>>> work = new ArrayDeque<>();
            enter(root);
            work.push(Map.entry(root, edges.get(root).iterator()));
            while (!work.isEmpty()) {
                Map.Entry<String, java.util.Iterator<String>> frame = work.peek();
                String v = frame.getKey();
                java.util.Iterator<String> it = frame.getValue();
                if (it.hasNext()) {
                    String w = it.next();
                    if (!index.containsKey(w)) {
                        enter(w);
                        work.push(Map.entry(w, edges.get(w).iterator()));
                    } else if (onStack.contains(w)) {
                        low.put(v, Math.min(low.get(v), index.get(w)));
                    }
                    continue;
                }
                work.pop();
                if (!work.isEmpty()) {
                    String parent = work.peek().getKey();
                    low.put(parent, Math.min(low.get(parent), low.get(v)));
                }
                if (low.get(v).equals(index.get(v))) {
                    List<String> component = new ArrayList<>();
                    String w;
                    do {
                        w = stack.pop();
                        onStack.remove(w);
                        component.add(w);
                    } while (!w.equals(v));
                    components.add(component);
                }
            }
        }

        private void enter(String v) {
            index.put(v, counter);
            low.put(v, counter);
            counter++;
            stack.push(v);
            onStack.add(v);
        }
    }

    // ---- resolution and order -------------------------------------------------------------------

    private void resolveAndOrder() {
        for (Block b : doc.blocks()) {
            boolean on = true;
            for (Condition c : b.when()) {
                on &= c.holds(value(c.input()));
            }
            if (on) {
                enabled.add(b.id());
            }
        }
        // Resolve each enabled block's bindings, in catalog order.
        Map<String, Map<String, Binding>> resolved = new HashMap<>();
        for (Block b : doc.blocks()) {
            if (!enabled.contains(b.id())) {
                continue;
            }
            Map<String, Binding> args = new LinkedHashMap<>();
            for (ArgSpec a : commands.get(b.id()).args()) {
                Binding binding = b.args().get(a.name());
                if (binding != null) {
                    resolve(binding).ifPresent(r -> args.put(a.name(), r));
                }
            }
            resolved.put(b.id(), args);
        }
        // Order: Kahn's algorithm, ties by document position.
        Map<String, Set<String>> edges = new HashMap<>();
        Map<String, Integer> indegree = new HashMap<>();
        for (String id : enabled) {
            edges.put(id, new HashSet<>());
            indegree.put(id, 0);
        }
        for (String id : enabled) {
            for (Binding r : resolved.get(id).values()) {
                if (r instanceof Binding.Output o && edges.get(o.block()).add(id)) {
                    indegree.merge(id, 1, Integer::sum);
                }
            }
        }
        PriorityQueue<String> ready = new PriorityQueue<>((a, b) -> Integer.compare(blockIndex.get(a), blockIndex.get(b)));
        indegree.forEach((id, d) -> {
            if (d == 0) {
                ready.add(id);
            }
        });
        List<String> ids = new ArrayList<>();
        while (!ready.isEmpty()) {
            String id = ready.poll();
            ids.add(id);
            for (String t : edges.get(id)) {
                if (indegree.merge(t, -1, Integer::sum) == 0) {
                    ready.add(t);
                }
            }
        }
        // Walk the order with section 01's latest-value rule (01.C16).
        Map<ValueKind, String> latest = new EnumMap<>(ValueKind.class);
        for (String id : ids) {
            Block b = doc.blocks().get(blockIndex.get(id));
            CommandSpec c = commands.get(id);
            Map<String, Binding> args = resolved.get(id);
            Set<String> deps = new TreeSet<>();
            // Every step reads the run's maps (D1's workspace), so a start block stays with any step.
            String maps = latest.get(ValueKind.MAPS);
            if (maps != null && !maps.startsWith("load:")) {
                deps.add(maps);
            }
            for (ArgSpec a : c.args()) {
                checkArgument(b, c, a, args.get(a.name()), latest, deps);
            }
            for (ValueKind needed : c.needs()) {
                String producer = latest.get(needed);
                if (producer == null) {
                    problems.add(PipelineProblem.forBlock(Severity.ERROR, id, "pipeline.missing_value",
                            "block " + id + ": " + c.name() + " needs " + article(needed)
                                    + ", which no block before it makes"));
                } else if (!producer.startsWith("load:")) {
                    deps.add(producer);
                }
            }
            c.produces().ifPresent(k -> latest.put(k, id));
            Planned p = new Planned(b, blockIndex.get(id), c, args, deps);
            order.add(p);
            planned.put(id, p);
        }
    }

    private void checkArgument(Block b, CommandSpec c, ArgSpec a, Binding r, Map<ValueKind, String> latest,
                               Set<String> deps) {
        String id = b.id();
        Optional<String> boundInput = inputOf(b.args().get(a.name()));
        if (r == null) {
            Optional<String> implicit = a.takes().map(latest::get);
            if (implicit.isPresent()) {
                if (!implicit.get().startsWith("load:")) {
                    deps.add(implicit.get());
                }
            } else if (a.need() == Need.REQUIRED) {
                // S8.20's stop on an unfilled value; FuSION 1 left three placeholders unfilled (S8.17).
                String why = boundInput.map(in -> label(in) + " is not set").orElse("nothing binds it");
                problems.add(PipelineProblem.forArgument(Severity.ERROR, id, a.name(), boundInput,
                        "pipeline.unbound_required", c.name() + " needs " + a.name() + ", but " + why));
            }
            return;
        }
        switch (r) {
            case Binding.Output out -> {
                Optional<ValueKind> made = commands.get(out.block()).produces();
                if (a.takes().isEmpty() || made.isEmpty() || made.get() != a.takes().get()) {
                    problems.add(PipelineProblem.forArgument(Severity.ERROR, id, a.name(), Optional.empty(),
                            "pipeline.kind_mismatch", "block " + id + ": " + a.name() + " takes "
                                    + a.takes().map(PipelinePlan::article).orElse("no in-memory value")
                                    + ", but block " + out.block() + " makes "
                                    + made.map(PipelinePlan::article).orElse("nothing")));
                } else if (!out.block().equals(latest.get(made.get()))) {
                    problems.add(PipelineProblem.forArgument(Severity.ERROR, id, a.name(), Optional.empty(),
                            "pipeline.not_latest", "block " + id + ": " + a.name() + " takes the latest "
                                    + Wire.name(made.get()) + " before it, which block " + latest.get(made.get())
                                    + " makes, not block " + out.block()));
                } else {
                    deps.add(out.block());
                }
            }
            case Binding.Saved s -> {
                if (a.takes().isEmpty()) {
                    problems.add(PipelineProblem.forArgument(Severity.ERROR, id, a.name(), Optional.empty(),
                            "pipeline.kind_mismatch", "block " + id + ": " + a.name()
                                    + " takes no in-memory value, so it cannot take a saved step"));
                } else {
                    latest.put(a.takes().get(), "load:" + s.runDir() + ":" + s.step());
                }
            }
            case Binding.Input in -> {
                InputDecl d = doc.input(in.name()).orElseThrow();
                if (d.list() && !a.list()) {
                    problems.add(PipelineProblem.forArgument(Severity.ERROR, id, a.name(), Optional.of(d.name()),
                            "pipeline.list_mismatch", "block " + id + ": " + a.name()
                                    + " takes one value, but input " + d.name() + " is a list"));
                }
                if (!compatible(d.type(), a.type())) {
                    problems.add(PipelineProblem.forArgument(Severity.ERROR, id, a.name(), Optional.of(d.name()),
                            "pipeline.type_mismatch", "block " + id + ": " + a.name() + " is "
                                    + Wire.name(a.type()) + ", but input " + d.name() + " is " + Wire.name(d.type())));
                }
            }
            case Binding.Literal l -> {
                if (!literalFits(l.value(), a.type())) {
                    problems.add(PipelineProblem.forArgument(Severity.ERROR, id, a.name(), Optional.empty(),
                            "pipeline.type_mismatch", "block " + id + ": " + a.name() + " is "
                                    + Wire.name(a.type()) + ", but its literal is " + l.value().text()));
                }
            }
            case Binding.Overrides o -> {
                if (!a.type().isPath()) {
                    problems.add(PipelineProblem.forArgument(Severity.ERROR, id, a.name(), Optional.empty(),
                            "pipeline.type_mismatch", "block " + id + ": " + a.name()
                                    + " is not a file, so it cannot take an override list"));
                }
            }
            case Binding.FirstOf f -> throw new IllegalStateException("resolved bindings hold no first_of");
        }
    }

    /** The first option that is bound, with {@code first_of} flattened. */
    Optional<Binding> resolve(Binding binding) {
        return switch (binding) {
            case Binding.Literal l -> l.value().isGiven() ? Optional.of(l) : Optional.empty();
            case Binding.Input in -> value(in.name()).isGiven() ? Optional.of(in) : Optional.empty();
            case Binding.Output out -> enabled.contains(out.block()) ? Optional.of(out) : Optional.empty();
            case Binding.Saved s -> Optional.of(s);
            case Binding.Overrides o -> overrideKinds.contains(o.kind()) ? Optional.of(o) : Optional.empty();
            case Binding.FirstOf f -> {
                for (Binding option : f.options()) {
                    Optional<Binding> r = resolve(option);
                    if (r.isPresent()) {
                        yield r;
                    }
                }
                yield Optional.empty();
            }
        };
    }

    /** The input a binding names, or the first one in a {@code first_of}. */
    private static Optional<String> inputOf(Binding binding) {
        if (binding instanceof Binding.Input in) {
            return Optional.of(in.name());
        }
        if (binding instanceof Binding.FirstOf f) {
            for (Binding option : f.options()) {
                Optional<String> found = inputOf(option);
                if (found.isPresent()) {
                    return found;
                }
            }
        }
        return Optional.empty();
    }

    /**
     * An input may feed an argument of its own type, and a path may feed a text argument, which takes
     * any token: fusion types {@code kgml_dir} as text (02.C12), where the frontend's input is a folder,
     * with a folder dialog and a folder check (10.C7).
     */
    static boolean compatible(ArgType input, ArgType argument) {
        return input == argument || input.isPath() && argument == ArgType.TEXT;
    }

    private static boolean literalFits(InputValue v, ArgType type) {
        return switch (type) {
            case BOOLEAN -> v instanceof InputValue.Flag;
            case INTEGER -> v instanceof InputValue.Int;
            case REAL -> v instanceof InputValue.Int || v instanceof InputValue.Real;
            case DURATION, TEXT, CHOICE -> v instanceof InputValue.Text || v instanceof InputValue.Choice
                    || v instanceof InputValue.GeneId || v instanceof InputValue.Items;
            case INPUT_FILE, INPUT_LIST, OUTPUT_NAME -> v instanceof InputValue.Text
                    || v instanceof InputValue.PathValue || v instanceof InputValue.InstallPath;
        };
    }

    String label(String input) {
        return doc.input(input).map(InputDecl::label).orElse(input);
    }

    private static String article(ValueKind kind) {
        String name = Wire.name(kind);
        return switch (kind) {
            case MAPS -> "the maps";
            case OBSERVATIONS, RESULTS -> name;
            default -> "a " + name;
        };
    }

    /**
     * The blocks that {@code stopAfter} depends on, with it: its connections, the latest values of
     * the kinds it needs, and theirs in turn (decision 0019's stop after a step).
     */
    Set<String> closure(String stopAfter) {
        Set<String> keep = new HashSet<>();
        Deque<String> todo = new ArrayDeque<>();
        todo.add(stopAfter);
        while (!todo.isEmpty()) {
            String id = todo.poll();
            if (keep.add(id)) {
                todo.addAll(planned.get(id).dependsOn());
            }
        }
        return keep;
    }
}
