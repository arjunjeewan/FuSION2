package fusion.frontend.pipeline;

import java.nio.file.InvalidPathException;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.OptionalDouble;

import tools.jackson.databind.JsonNode;
import tools.jackson.databind.node.ArrayNode;
import tools.jackson.databind.node.ObjectNode;

/**
 * Reads and writes pipeline documents, input values and gene overrides in the JSON of section 10
 * (The pipeline document; Files this section writes). A value is typed by its declaration: the JSON
 * string {@code "hsa1956"} is a gene id for an input with the {@code gene_id} hint, a path for an
 * {@code input_file} input and a choice for a {@code choice} input.
 */
public final class PipelineJson {
    private PipelineJson() {}

    // ---- values ---------------------------------------------------------------------------------

    public static JsonNode writeValue(InputValue value) {
        return switch (value) {
            case InputValue.Text t -> Json.MAPPER.getNodeFactory().stringNode(t.value());
            case InputValue.Int i -> Json.MAPPER.getNodeFactory().numberNode(i.value());
            case InputValue.Real r -> Json.MAPPER.getNodeFactory().numberNode(r.value());
            case InputValue.PathValue p -> Json.MAPPER.getNodeFactory().stringNode(p.value().toString());
            case InputValue.InstallPath i -> Json.object().put("install", i.relative());
            case InputValue.GeneId g -> Json.MAPPER.getNodeFactory().stringNode(g.value());
            case InputValue.Choice c -> Json.MAPPER.getNodeFactory().stringNode(c.value());
            case InputValue.Flag f -> Json.MAPPER.getNodeFactory().booleanNode(f.value());
            case InputValue.Items l -> {
                ArrayNode a = Json.array();
                l.items().forEach(item -> a.add(writeValue(item)));
                yield a;
            }
            case InputValue.Sweep s -> {
                ObjectNode o = Json.object();
                ArrayNode a = o.putArray("sweep");
                s.values().forEach(item -> a.add(writeValue(item)));
                yield o;
            }
            case InputValue.Unset u -> Json.MAPPER.getNodeFactory().nullNode();
        };
    }

    /** A value of a declared input, typed by its declaration. */
    public static InputValue readValue(JsonNode node, InputDecl decl) {
        if (node == null || node.isNull() || node.isMissingNode()) {
            return InputValue.UNSET;
        }
        if (node.isObject() && node.has("sweep")) {
            List<InputValue> values = new ArrayList<>();
            for (JsonNode v : array(node.get("sweep"), "input " + decl.name() + ": sweep")) {
                values.add(readListOrScalar(v, decl));
            }
            return new InputValue.Sweep(values);
        }
        return readListOrScalar(node, decl);
    }

    private static InputValue readListOrScalar(JsonNode node, InputDecl decl) {
        if (!decl.list()) {
            return readScalar(node, decl);
        }
        List<InputValue> items = new ArrayList<>();
        if (node.isArray()) {
            for (JsonNode item : node) {
                items.add(readScalar(item, decl));
            }
        } else {
            items.add(readScalar(node, decl));
        }
        return new InputValue.Items(items);
    }

    private static InputValue readScalar(JsonNode node, InputDecl decl) {
        String what = "input " + decl.name();
        if (node.isNull()) {
            return InputValue.UNSET;
        }
        switch (decl.type()) {
            case BOOLEAN -> {
                if (node.isBoolean()) {
                    return new InputValue.Flag(node.booleanValue());
                }
            }
            case INTEGER -> {
                if (node.isNumber() && node.canConvertToExactIntegral() && node.canConvertToLong()) {
                    return new InputValue.Int(node.asLong());
                }
            }
            case REAL -> {
                if (node.isNumber()) {
                    return new InputValue.Real(node.asDouble());
                }
            }
            case DURATION -> {
                if (node.isString()) {
                    return new InputValue.Text(node.stringValue());
                }
            }
            case TEXT -> {
                if (node.isString()) {
                    return decl.hint().filter(h -> h == InputHint.GENE_ID).isPresent()
                            ? new InputValue.GeneId(node.stringValue())
                            : new InputValue.Text(node.stringValue());
                }
            }
            case CHOICE -> {
                if (node.isString()) {
                    return new InputValue.Choice(node.stringValue());
                }
            }
            case INPUT_FILE, INPUT_LIST, OUTPUT_NAME -> {
                if (node.isString()) {
                    return new InputValue.PathValue(path(node.stringValue(), what));
                }
                if (node.isObject() && node.has("install")) {
                    return new InputValue.InstallPath(Json.requiredText(node, "install", what));
                }
            }
        }
        throw new PipelineException("json.bad_value",
                what + ": " + Json.compact(node) + " is not a " + Wire.name(decl.type()) + " value");
    }

    /** A value with no declaration to type it: a literal, a condition's value or an unused input. */
    public static InputValue readUntyped(JsonNode node) {
        if (node == null || node.isNull() || node.isMissingNode()) {
            return InputValue.UNSET;
        }
        if (node.isBoolean()) {
            return new InputValue.Flag(node.booleanValue());
        }
        if (node.isIntegralNumber() && node.canConvertToLong()) {
            return new InputValue.Int(node.asLong());
        }
        if (node.isNumber()) {
            return new InputValue.Real(node.asDouble());
        }
        if (node.isString()) {
            return new InputValue.Text(node.stringValue());
        }
        if (node.isArray()) {
            List<InputValue> items = new ArrayList<>();
            node.forEach(item -> items.add(readUntyped(item)));
            return new InputValue.Items(items);
        }
        if (node.isObject() && node.has("install")) {
            return new InputValue.InstallPath(Json.requiredText(node, "install", "value"));
        }
        if (node.isObject() && node.has("sweep")) {
            List<InputValue> values = new ArrayList<>();
            array(node.get("sweep"), "sweep").forEach(item -> values.add(readUntyped(item)));
            return new InputValue.Sweep(values);
        }
        throw new PipelineException("json.bad_value", "not a value: " + Json.compact(node));
    }

    /** Values keyed by input name; declared inputs are typed, others are kept untyped (10.C8). */
    public static Map<String, InputValue> readValues(JsonNode node, List<InputDecl> decls) {
        Map<String, InputValue> values = new LinkedHashMap<>();
        if (node == null || node.isNull()) {
            return values;
        }
        if (!node.isObject()) {
            throw new PipelineException("json.bad_value", "inputs must be an object");
        }
        for (Map.Entry<String, JsonNode> e : node.properties()) {
            Optional<InputDecl> decl = decls.stream().filter(d -> d.name().equals(e.getKey())).findFirst();
            values.put(e.getKey(), decl.isPresent() ? readValue(e.getValue(), decl.get()) : readUntyped(e.getValue()));
        }
        return values;
    }

    /** Values keyed by name; an unset value stays as an explicit null (S8.05). */
    public static ObjectNode writeValues(Map<String, InputValue> values) {
        ObjectNode o = Json.object();
        values.forEach((name, value) -> o.set(name, writeValue(value)));
        return o;
    }

    // ---- documents ------------------------------------------------------------------------------

    public static PipelineDocument read(String text, String what) {
        return read(Json.parse(text, what));
    }

    public static PipelineDocument read(JsonNode root) {
        if (!root.isObject()) {
            throw new PipelineException("json.bad_value", "a pipeline must be a JSON object");
        }
        int format = root.path("format").asInt(0);
        if (format != PipelineDocument.FORMAT) {
            throw new PipelineException("pipeline.format",
                    "pipeline format " + format + " is not the format this frontend reads ("
                            + PipelineDocument.FORMAT + ")");
        }
        String origin = Json.text(root, "origin").orElse("");
        List<InputDecl> inputs = new ArrayList<>();
        for (JsonNode n : array(root.get("inputs"), "inputs")) {
            inputs.add(readDecl(n));
        }
        List<Block> blocks = new ArrayList<>();
        for (JsonNode n : array(root.get("blocks"), "blocks")) {
            blocks.add(readBlock(n));
        }
        return new PipelineDocument(format, origin, inputs, blocks);
    }

    public static ObjectNode write(PipelineDocument doc) {
        ObjectNode root = Json.object();
        root.put("format", doc.format());
        root.put("origin", doc.origin());
        ArrayNode inputs = root.putArray("inputs");
        doc.inputs().forEach(d -> inputs.add(writeDecl(d)));
        ArrayNode blocks = root.putArray("blocks");
        doc.blocks().forEach(b -> blocks.add(writeBlock(b)));
        return root;
    }

    /** The SHA-256 of the document's canonical JSON, which a batch's header records. */
    public static String sha256(PipelineDocument doc) {
        return Sha256.of(Json.canonical(write(doc)));
    }

    static InputDecl readDecl(JsonNode n) {
        String name = Json.requiredText(n, "name", "an input");
        String what = "input " + name;
        ArgType type = Wire.parse(ArgType.class, Json.requiredText(n, "type", what), what + ": type");
        boolean list = Json.flag(n, "list", false);
        Optional<InputHint> hint = Json.text(n, "hint").map(h -> Wire.parse(InputHint.class, h, what + ": hint"));
        // A shell declaration types the default.
        InputDecl shell = new InputDecl(name, type, list, hint, false, Optional.empty(), Optional.empty(),
                OptionalDouble.empty(), OptionalDouble.empty(), List.of(), false, false, name, "", "");
        JsonNode def = n.get("default");
        Optional<InputValue> defaultValue = def == null || def.isNull()
                ? Optional.empty() : Optional.of(readValue(def, shell));
        return new InputDecl(name, type, list, hint, Json.flag(n, "required", false), defaultValue,
                Json.text(n, "unit"), number(n, "min", what), number(n, "max", what), strings(n.get("choices"), what),
                Json.flag(n, "sweepable", false), Json.flag(n, "remember", false),
                Json.text(n, "label").orElse(name), Json.text(n, "group").orElse(""),
                Json.text(n, "help").orElse(""));
    }

    static ObjectNode writeDecl(InputDecl d) {
        ObjectNode o = Json.object();
        o.put("name", d.name());
        o.put("type", Wire.name(d.type()));
        if (d.list()) {
            o.put("list", true);
        }
        d.hint().ifPresent(h -> o.put("hint", Wire.name(h)));
        if (d.required()) {
            o.put("required", true);
        }
        d.defaultValue().ifPresent(v -> o.set("default", writeValue(v)));
        d.unit().ifPresent(u -> o.put("unit", u));
        d.min().ifPresent(m -> o.put("min", m));
        d.max().ifPresent(m -> o.put("max", m));
        if (!d.choices().isEmpty()) {
            ArrayNode c = o.putArray("choices");
            d.choices().forEach(c::add);
        }
        if (d.sweepable()) {
            o.put("sweepable", true);
        }
        if (d.remember()) {
            o.put("remember", true);
        }
        o.put("label", d.label());
        if (!d.group().isEmpty()) {
            o.put("group", d.group());
        }
        if (!d.help().isEmpty()) {
            o.put("help", d.help());
        }
        return o;
    }

    static Block readBlock(JsonNode n) {
        String id = Json.requiredText(n, "id", "a block");
        String what = "block " + id;
        String command = Json.requiredText(n, "command", what);
        Map<String, Binding> args = new LinkedHashMap<>();
        JsonNode argsNode = n.get("args");
        if (argsNode != null && !argsNode.isNull()) {
            if (!argsNode.isObject()) {
                throw new PipelineException("json.bad_value", what + ": args must be an object");
            }
            for (Map.Entry<String, JsonNode> e : argsNode.properties()) {
                args.put(e.getKey(), readBinding(e.getValue(), what + ", argument " + e.getKey()));
            }
        }
        List<Condition> when = new ArrayList<>();
        for (JsonNode c : array(n.get("when"), what + ": when")) {
            String input = Json.requiredText(c, "input", what + ": condition");
            JsonNode eq = c.get("equals");
            when.add(new Condition(input, eq == null ? Optional.empty() : Optional.of(readUntyped(eq))));
        }
        return new Block(id, command, args, when, n.path("x").asDouble(0), n.path("y").asDouble(0));
    }

    static ObjectNode writeBlock(Block b) {
        ObjectNode o = Json.object();
        o.put("id", b.id());
        o.put("command", b.command());
        if (!b.when().isEmpty()) {
            ArrayNode when = o.putArray("when");
            for (Condition c : b.when()) {
                ObjectNode co = when.addObject();
                co.put("input", c.input());
                c.equals().ifPresent(v -> co.set("equals", writeValue(v)));
            }
        }
        ObjectNode args = o.putObject("args");
        b.args().forEach((name, binding) -> args.set(name, writeBinding(binding)));
        o.put("x", b.x());
        o.put("y", b.y());
        return o;
    }

    static Binding readBinding(JsonNode n, String what) {
        if (n == null || !n.isObject() || n.size() != 1) {
            throw new PipelineException("json.bad_value",
                    what + ": a binding is an object with one of literal, input, output, saved, first_of or overrides");
        }
        String kind = n.propertyNames().iterator().next();
        JsonNode v = n.get(kind);
        return switch (kind) {
            case "literal" -> new Binding.Literal(readUntyped(v));
            case "input" -> new Binding.Input(Json.requiredText(n, "input", what));
            case "output" -> new Binding.Output(Json.requiredText(n, "output", what));
            case "saved" -> new Binding.Saved(path(Json.requiredText(v, "run", what), what),
                                              Json.requiredText(v, "step", what));
            case "first_of" -> {
                List<Binding> options = new ArrayList<>();
                for (JsonNode option : array(v, what)) {
                    options.add(readBinding(option, what));
                }
                yield new Binding.FirstOf(options);
            }
            case "overrides" -> new Binding.Overrides(Json.requiredText(n, "overrides", what));
            default -> throw new PipelineException("json.bad_value", what + ": unknown binding '" + kind + "'");
        };
    }

    static ObjectNode writeBinding(Binding b) {
        ObjectNode o = Json.object();
        switch (b) {
            case Binding.Literal l -> o.set("literal", writeValue(l.value()));
            case Binding.Input i -> o.put("input", i.name());
            case Binding.Output out -> o.put("output", out.block());
            case Binding.Saved s -> {
                ObjectNode saved = o.putObject("saved");
                saved.put("run", s.runDir().toString());
                saved.put("step", s.step());
            }
            case Binding.FirstOf f -> {
                ArrayNode options = o.putArray("first_of");
                f.options().forEach(option -> options.add(writeBinding(option)));
            }
            case Binding.Overrides ov -> o.put("overrides", ov.kind());
        }
        return o;
    }

    // ---- overrides ------------------------------------------------------------------------------

    public static ArrayNode writeOverrides(List<GeneOverride> overrides) {
        ArrayNode a = Json.array();
        for (GeneOverride g : overrides) {
            ObjectNode o = a.addObject();
            o.put("gene_id", g.geneId());
            putEnum(o, "observation", g.observation());
            putEnum(o, "activity", g.activity());
            putEnum(o, "relaxation", g.relaxation());
        }
        return a;
    }

    public static List<GeneOverride> readOverrides(JsonNode node) {
        List<GeneOverride> out = new ArrayList<>();
        for (JsonNode o : array(node, "overrides")) {
            String gene = Json.requiredText(o, "gene_id", "an override");
            out.add(new GeneOverride(gene,
                    Json.text(o, "observation").map(t -> Wire.parse(Observation.class, t, "override observation")),
                    Json.text(o, "activity").map(t -> Wire.parse(Activity.class, t, "override activity")),
                    Json.text(o, "relaxation").map(t -> Wire.parse(Relaxation.class, t, "override relaxation"))));
        }
        return out;
    }

    private static void putEnum(ObjectNode o, String member, Optional<? extends Enum<?>> value) {
        if (value.isPresent()) {
            o.put(member, Wire.name(value.get()));
        } else {
            o.putNull(member);
        }
    }

    // ---- helpers --------------------------------------------------------------------------------

    static List<JsonNode> array(JsonNode node, String what) {
        List<JsonNode> out = new ArrayList<>();
        if (node == null || node.isNull() || node.isMissingNode()) {
            return out;
        }
        if (!node.isArray()) {
            throw new PipelineException("json.bad_value", what + " must be an array");
        }
        node.forEach(out::add);
        return out;
    }

    static List<String> strings(JsonNode node, String what) {
        List<String> out = new ArrayList<>();
        for (JsonNode n : array(node, what)) {
            if (!n.isString()) {
                throw new PipelineException("json.bad_value", what + ": expected strings");
            }
            out.add(n.stringValue());
        }
        return out;
    }

    static OptionalDouble number(JsonNode n, String member, String what) {
        JsonNode v = n.get(member);
        if (v == null || v.isNull()) {
            return OptionalDouble.empty();
        }
        if (!v.isNumber()) {
            throw new PipelineException("json.bad_value", what + ": '" + member + "' must be a number");
        }
        return OptionalDouble.of(v.asDouble());
    }

    private static Path path(String text, String what) {
        try {
            return Path.of(text);
        } catch (InvalidPathException e) {
            throw new PipelineException("json.bad_value", what + ": not a path: " + text, e);
        }
    }
}
