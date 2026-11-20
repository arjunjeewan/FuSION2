package fusion.frontend.pipeline;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.OptionalDouble;
import java.util.Set;
import java.util.regex.Pattern;

/**
 * Section 10's validation steps 1 and 2 (S8.30, decision 0021): the inputs against their
 * declarations and the catalog's cross-argument rules, then the pipeline (unknown commands and
 * arguments, required arguments left unbound, kind mismatches, connections that are not the latest
 * of their kind, cycles). The frontend evaluates the same rules that {@code fusion} enforces
 * (01.C9). FuSION 1 checked nothing before a run ({@code Linux/frontend/src/ScriptFile.java:24-82};
 * S8.30), stored 0 for an unreadable solver field ({@code ReachPathBoundCard.java:153-163}; S8.10)
 * and kept a typed id with no match ({@code IdEntryPanel.java:406-421}; S8.12).
 */
public final class StandardPipelineValidator implements PipelineValidator {
    /** A bare gene id: one token, no comma, quote or {@code $} (S8.12, S1.08). */
    static final Pattern GENE_ID = Pattern.compile("[^\\s,\"$]+");

    @Override
    public List<PipelineProblem> validate(PipelineDocument pipeline, Map<String, InputValue> inputs, Catalog catalog) {
        PipelinePlan plan = PipelinePlan.of(pipeline, inputs, Set.of(), catalog);
        List<PipelineProblem> out = new ArrayList<>();
        // Step 1: each input against its declaration, then the values against the catalog's
        // arguments and rules. Each part, and step 2, runs only once the one before has passed
        // (section 10, Validation), so one fault is not reported twice.
        for (InputDecl d : pipeline.inputs()) {
            checkInput(d, plan.value(d.name()), out);
        }
        plan.undeclared.forEach((name, value) -> out.add(PipelineProblem.forInput(Severity.WARNING, name,
                "input.unused", "input " + name + " is kept, but this pipeline no longer uses it")));
        if (out.stream().anyMatch(PipelineProblem::isError)) {
            return out;
        }
        checkArguments(plan, out);
        if (out.stream().anyMatch(PipelineProblem::isError)) {
            return out;
        }
        // Step 2: the pipeline.
        out.addAll(plan.problems);
        return out;
    }

    // ---- step 1: each input against its declaration --------------------------------------------

    static void checkInput(InputDecl d, InputValue v, List<PipelineProblem> out) {
        if (!v.isGiven()) {
            if (d.required()) {
                out.add(problem(d, "input.required", d.label() + " is required"));
            }
            return;
        }
        if (v instanceof InputValue.Sweep s) {
            if (!d.sweepable()) {
                out.add(problem(d, "input.not_sweepable", d.label() + " cannot be swept"));
                return;
            }
            for (InputValue one : s.values()) {
                checkListOrScalar(d, one, out);
            }
            return;
        }
        checkListOrScalar(d, v, out);
    }

    private static void checkListOrScalar(InputDecl d, InputValue v, List<PipelineProblem> out) {
        if (d.list()) {
            if (!(v instanceof InputValue.Items items)) {
                out.add(problem(d, "input.bad_type", d.label() + " takes a list of values"));
                return;
            }
            for (InputValue item : items.items()) {
                if (item.text().indexOf(',') >= 0) {
                    // A list is written joined by commas (01's list mark), so an item cannot hold one.
                    out.add(problem(d, "input.comma_in_item", d.label() + ": '" + item.text() + "' holds a comma"));
                } else {
                    checkScalar(d, item, out);
                }
            }
            return;
        }
        if (v instanceof InputValue.Items) {
            out.add(problem(d, "input.bad_type", d.label() + " takes one value, not a list"));
            return;
        }
        checkScalar(d, v, out);
    }

    private static void checkScalar(InputDecl d, InputValue v, List<PipelineProblem> out) {
        if (hasControl(v.text())) {
            out.add(problem(d, "input.control_character", d.label() + " holds a control character"));
            return;
        }
        boolean fits = switch (d.type()) {
            case BOOLEAN -> v instanceof InputValue.Flag;
            case INTEGER -> v instanceof InputValue.Int;
            case REAL -> v instanceof InputValue.Real || v instanceof InputValue.Int;
            case DURATION -> v instanceof InputValue.Text t && BatchText.DURATION.matcher(t.value()).matches();
            case TEXT -> v instanceof InputValue.Text || v instanceof InputValue.GeneId;
            case CHOICE -> v instanceof InputValue.Choice || v instanceof InputValue.Text;
            case INPUT_FILE, INPUT_LIST, OUTPUT_NAME -> v instanceof InputValue.PathValue
                    || v instanceof InputValue.InstallPath;
        };
        if (!fits) {
            String expected = switch (d.type()) {
                case DURATION -> "a duration with a unit, such as 4h, 90s or 500ms";
                case INTEGER -> "a whole number";
                case REAL -> "a number";
                case BOOLEAN -> "yes or no";
                case INPUT_FILE, INPUT_LIST, OUTPUT_NAME -> "a path";
                default -> "text";
            };
            out.add(problem(d, "input.bad_type", d.label() + " must be " + expected + ", not '" + v.text() + "'"));
            return;
        }
        if (v instanceof InputValue.PathValue p && !p.value().isAbsolute()) {
            out.add(problem(d, "input.relative_path", d.label() + " must be an absolute path: " + p.value()));
        }
        if (d.hint().filter(h -> h == InputHint.GENE_ID).isPresent() && !GENE_ID.matcher(v.text()).matches()) {
            // S8.12: bare ids only; FuSION 1 stored "hsa1956 - egfr" and relied on let's first token.
            out.add(problem(d, "input.bad_gene_id", d.label() + " must be a bare gene id, such as hsa1956, not '"
                    + v.text() + "'"));
        }
        if (!d.choices().isEmpty() && (d.type() == ArgType.CHOICE) && !d.choices().contains(v.text())) {
            out.add(problem(d, "input.bad_choice", d.label() + " must be one of " + String.join(", ", d.choices())));
        }
        range(d.label(), v, d.min(), d.max()).ifPresent(msg -> out.add(problem(d, msg[0], msg[1])));
    }

    /** A value outside {@code [min, max]}: its code and message. */
    static Optional<String[]> range(String label, InputValue v, OptionalDouble min, OptionalDouble max) {
        Optional<Double> n = number(v);
        if (n.isEmpty()) {
            return Optional.empty();
        }
        if (min.isPresent() && n.get() < min.getAsDouble()) {
            return Optional.of(new String[] {"input.below_min",
                    label + " must be at least " + InputValue.formatReal(min.getAsDouble())});
        }
        if (max.isPresent() && n.get() > max.getAsDouble()) {
            return Optional.of(new String[] {"input.above_max",
                    label + " must be at most " + InputValue.formatReal(max.getAsDouble())});
        }
        return Optional.empty();
    }

    static Optional<Double> number(InputValue v) {
        if (v instanceof InputValue.Int i) {
            return Optional.of((double) i.value());
        }
        if (v instanceof InputValue.Real r) {
            return Optional.of(r.value());
        }
        return Optional.empty();
    }

    static boolean hasControl(String text) {
        return text.codePoints().anyMatch(BatchText::isControl);
    }

    private static PipelineProblem problem(InputDecl d, String code, String message) {
        return PipelineProblem.forInput(Severity.ERROR, d.name(), code, message);
    }

    // ---- step 1: values against the catalog's arguments and rules -------------------------------

    /** What one argument of an enabled block receives: given or not, and the values to compare. */
    private record ArgValue(boolean given, List<InputValue> values, Optional<String> input, String label) {}

    private static void checkArguments(PipelinePlan plan, List<PipelineProblem> out) {
        for (PipelinePlan.Planned p : plan.order) {
            String id = p.block().id();
            for (ArgSpec a : p.command().args()) {
                ArgValue av = argValue(plan, p, a.name());
                for (InputValue v : av.values()) {
                    Optional<String[]> bad = range(av.label(), v, a.min(), a.max());
                    if (bad.isPresent()) {
                        out.add(PipelineProblem.forArgument(Severity.ERROR, id, a.name(), av.input(),
                                bad.get()[0].replace("input.", "argument."),
                                bad.get()[1] + " (" + p.command().name() + " " + a.name() + ")"));
                    } else if (!a.choices().isEmpty() && a.type() == ArgType.CHOICE
                            && !a.choices().contains(v.text())) {
                        out.add(PipelineProblem.forArgument(Severity.ERROR, id, a.name(), av.input(),
                                "argument.bad_choice", av.label() + " must be one of "
                                        + String.join(", ", a.choices()) + " (" + p.command().name() + " " + a.name() + ")"));
                    }
                }
            }
            for (Rule r : p.command().rules()) {
                checkRule(plan, p, r, out);
            }
        }
    }

    private static void checkRule(PipelinePlan plan, PipelinePlan.Planned p, Rule r, List<PipelineProblem> out) {
        ArgValue a = argValue(plan, p, r.a());
        ArgValue b = argValue(plan, p, r.b());
        String id = p.block().id();
        Optional<String> code = Optional.empty();
        String message = "";
        switch (r.kind()) {
            case LESS, LESS_OR_EQUAL -> {
                boolean strict = r.kind() == RuleKind.LESS;
                for (InputValue x : a.values()) {
                    for (InputValue y : b.values()) {
                        Optional<Double> nx = number(x);
                        Optional<Double> ny = number(y);
                        if (nx.isPresent() && ny.isPresent()
                                && (strict ? nx.get() >= ny.get() : nx.get() > ny.get()) && code.isEmpty()) {
                            // S3.07: up must lie above down; S8.11: lower <= upper.
                            code = Optional.of(strict ? "rule.less" : "rule.less_or_equal");
                            message = a.label() + " (" + x.text() + ") must be " + (strict ? "below " : "at most ")
                                    + b.label() + " (" + y.text() + ")";
                        }
                    }
                }
            }
            case REQUIRES -> {
                if (a.given() && !b.given()) {
                    code = Optional.of("rule.requires");
                    message = a.label() + " needs " + b.label();
                }
            }
            case EXCLUDES -> {
                if (a.given() && b.given()) {
                    code = Optional.of("rule.excludes");
                    message = a.label() + " and " + b.label() + " cannot both be given";
                }
            }
        }
        if (code.isPresent()) {
            Optional<String> input = a.input().isPresent() ? a.input() : b.input();
            out.add(PipelineProblem.forArgument(Severity.ERROR, id, r.a(), input, code.get(),
                    message + " (" + p.command().name() + ": " + r + ")"));
        }
    }

    private static ArgValue argValue(PipelinePlan plan, PipelinePlan.Planned p, String argName) {
        Binding binding = p.args().get(argName);
        if (binding == null) {
            return new ArgValue(false, List.of(), Optional.empty(), argName);
        }
        return switch (binding) {
            case Binding.Literal l -> new ArgValue(true, List.of(l.value()), Optional.empty(), argName);
            case Binding.Input in -> {
                InputValue v = plan.value(in.name());
                List<InputValue> values = v instanceof InputValue.Sweep s ? s.values() : List.of(v);
                List<InputValue> scalars = new ArrayList<>();
                for (InputValue one : values) {
                    if (one instanceof InputValue.Items items) {
                        scalars.addAll(items.items());
                    } else {
                        scalars.add(one);
                    }
                }
                yield new ArgValue(true, scalars, Optional.of(in.name()), plan.label(in.name()));
            }
            default -> new ArgValue(true, List.of(), Optional.empty(), argName);
        };
    }
}
