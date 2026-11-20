package fusion.frontend.pipeline;

import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import tools.jackson.databind.JsonNode;

/**
 * Reads the output of {@code fusion --commands} (01.C9) into a {@link Catalog}: the backend's own
 * command table, with each command's {@code arguments} (name, setting, type, need, default, range,
 * choices, the {@code list} and {@code takes} marks), {@code needs}, {@code produces} and
 * {@code rules}, and the top-level {@code run_settings}. It ignores members it does not know
 * ({@code forms}, {@code tools}, {@code owner} and the rest). Left out, because no block can use
 * them: the language statements that are not steps ({@code let}, {@code sweep}, {@code run},
 * {@code exit}: the generator writes those), dropped commands such as {@code upfx} (S2.29), and
 * arguments that only FuSION 1's positional form takes. Stage names are display labels, which the
 * frontend keeps itself ({@link StageLabels}).
 */
public final class CatalogJson {
    private CatalogJson() {}

    private static final Pattern RULE_TEXT = Pattern.compile("\\s*([a-z_]+)\\s*\\(\\s*([a-z0-9_]+)\\s*,\\s*([a-z0-9_]+)\\s*\\)\\s*");

    public static Catalog read(String text) {
        return read(Json.parse(text, "fusion --commands"));
    }

    public static Catalog read(JsonNode root) {
        if (!root.isObject()) {
            throw new PipelineException("catalog.bad", "fusion --commands: expected a JSON object");
        }
        List<CommandSpec> commands = new ArrayList<>();
        for (JsonNode c : PipelineJson.array(root.get("commands"), "commands")) {
            boolean step = Json.flag(c, "step", true);
            boolean dropped = Json.text(c, "dropped").filter(d -> !d.isEmpty()).isPresent();
            if (step && !dropped) {
                commands.add(command(c));
            }
        }
        List<SettingSpec> settings = new ArrayList<>();
        JsonNode settingsNode = root.has("run_settings") ? root.get("run_settings") : root.get("settings");
        for (JsonNode s : PipelineJson.array(settingsNode, "run settings")) {
            String key = Json.requiredText(s, "key", "a run setting");
            settings.add(new SettingSpec(key, type(s, "run setting " + key), need(s, "run setting " + key),
                    defaultText(s), Json.text(s, "help").orElse("")));
        }
        // "format" names the format ("fusion.commands"); its version is "format_version".
        int format = root.has("format_version") ? root.get("format_version").asInt(1) : root.path("format").asInt(1);
        return new Catalog(format, Json.text(root, "fusion").orElse(""), commands, settings);
    }

    private static CommandSpec command(JsonNode c) {
        String name = Json.requiredText(c, "name", "a command");
        String what = "command " + name;
        List<ArgSpec> args = new ArrayList<>();
        JsonNode arguments = c.has("arguments") ? c.get("arguments") : c.get("args");
        for (JsonNode a : PipelineJson.array(arguments, what + ": arguments")) {
            if (!Json.flag(a, "positional_only", false)) {
                args.add(arg(a, what));
            }
        }
        List<Rule> rules = new ArrayList<>();
        for (JsonNode r : PipelineJson.array(c.get("rules"), what + ": rules")) {
            rules.add(rule(r, what));
        }
        List<ValueKind> needs = new ArrayList<>();
        for (String k : PipelineJson.strings(c.get("needs"), what + ": needs")) {
            needs.add(Wire.parse(ValueKind.class, k, what + ": needs"));
        }
        Optional<ValueKind> produces = Json.text(c, "produces")
                .map(k -> Wire.parse(ValueKind.class, k, what + ": produces"));
        String help = Json.text(c, "help").orElse(Json.text(c, "description").orElse(""));
        return new CommandSpec(name, PipelineJson.strings(c.get("aliases"), what + ": aliases"),
                Json.text(c, "stage").orElse(StageLabels.of(name)), Json.text(c, "summary").orElse(""), help,
                args, rules, needs, produces, Json.flag(c, "debug", false));
    }

    private static ArgSpec arg(JsonNode a, String command) {
        String name = Json.requiredText(a, "name", command + ": an argument");
        String what = command + ", argument " + name;
        Optional<ValueKind> takes = Json.text(a, "takes").map(k -> Wire.parse(ValueKind.class, k, what + ": takes"));
        return new ArgSpec(name, Json.text(a, "setting").orElse(name), type(a, what), Json.flag(a, "list", false),
                takes, need(a, what), defaultText(a), PipelineJson.number(a, "min", what),
                PipelineJson.number(a, "max", what), PipelineJson.strings(a.get("choices"), what + ": choices"),
                Json.flag(a, "debug", false), Json.text(a, "help").orElse(""));
    }

    private static ArgType type(JsonNode n, String what) {
        return Wire.parse(ArgType.class, Json.requiredText(n, "type", what), what + ": type");
    }

    private static Need need(JsonNode n, String what) {
        return Json.text(n, "need").map(t -> Wire.parse(Need.class, t, what + ": need")).orElse(Need.OPTIONAL);
    }

    private static Optional<String> defaultText(JsonNode n) {
        JsonNode d = n.get("default");
        if (d == null || d.isNull()) {
            return Optional.empty();
        }
        if (d.isString()) {
            return Optional.of(d.stringValue());
        }
        if (d.isBoolean()) {
            return Optional.of(d.booleanValue() ? "true" : "false");
        }
        if (d.isIntegralNumber()) {
            return Optional.of(d.bigIntegerValue().toString());
        }
        if (d.isNumber()) {
            return Optional.of(InputValue.formatReal(d.asDouble()));
        }
        if (d.isArray()) {
            List<String> items = new ArrayList<>();
            d.forEach(item -> items.add(item.asString()));
            return Optional.of(String.join(",", items));
        }
        return Optional.of(Json.compact(d));
    }

    private static Rule rule(JsonNode r, String what) {
        if (r.isString()) {
            Matcher m = RULE_TEXT.matcher(r.stringValue());
            if (!m.matches()) {
                throw new PipelineException("catalog.bad", what + ": cannot read rule '" + r.stringValue() + "'");
            }
            return new Rule(Wire.parse(RuleKind.class, m.group(1), what + ": rule"), m.group(2), m.group(3));
        }
        RuleKind kind = Wire.parse(RuleKind.class, Json.requiredText(r, "kind", what + ": rule"), what + ": rule");
        if (r.has("args")) {
            List<String> names = PipelineJson.strings(r.get("args"), what + ": rule");
            if (names.size() != 2) {
                throw new PipelineException("catalog.bad", what + ": a rule names two arguments");
            }
            return new Rule(kind, names.get(0), names.get(1));
        }
        return new Rule(kind, Json.requiredText(r, "a", what + ": rule"), Json.requiredText(r, "b", what + ": rule"));
    }
}
