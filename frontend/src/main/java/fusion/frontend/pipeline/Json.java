package fusion.frontend.pipeline;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

import tools.jackson.core.JacksonException;
import tools.jackson.core.StreamReadFeature;
import tools.jackson.core.util.DefaultIndenter;
import tools.jackson.core.util.DefaultPrettyPrinter;
import tools.jackson.core.util.Separators;
import tools.jackson.databind.JsonNode;
import tools.jackson.databind.ObjectWriter;
import tools.jackson.databind.json.JsonMapper;
import tools.jackson.databind.node.ArrayNode;
import tools.jackson.databind.node.ObjectNode;

/**
 * The frontend's one JSON setup (Jackson 3): duplicate keys are errors, so a document cannot bind
 * one argument twice, and files are written with {@code \n} line ends on every platform.
 */
public final class Json {
    private Json() {}

    public static final JsonMapper MAPPER = JsonMapper.builder()
            .enable(StreamReadFeature.STRICT_DUPLICATE_DETECTION)
            .build();

    private static final ObjectWriter PRETTY = MAPPER.writer().with(
            new DefaultPrettyPrinter(Separators.createDefaultInstance()
                    .withObjectNameValueSpacing(Separators.Spacing.AFTER))
                    .withObjectIndenter(new DefaultIndenter("  ", "\n"))
                    .withArrayIndenter(new DefaultIndenter("  ", "\n")));

    public static ObjectNode object() {
        return MAPPER.createObjectNode();
    }

    public static ArrayNode array() {
        return MAPPER.createArrayNode();
    }

    /** Parses JSON text; a {@link PipelineException} {@code json.parse} that names {@code what}. */
    public static JsonNode parse(String text, String what) {
        try {
            JsonNode node = MAPPER.readTree(text);
            if (node == null || node.isMissingNode()) {
                throw new PipelineException("json.parse", what + ": no JSON value");
            }
            return node;
        } catch (JacksonException e) {
            throw new PipelineException("json.parse", what + ": " + e.getOriginalMessage(), e);
        }
    }

    /** Indented, two spaces, {@code \n} line ends and a final {@code \n}. */
    public static String pretty(JsonNode node) {
        return PRETTY.writeValueAsString(node) + "\n";
    }

    /** One line, no spaces. */
    public static String compact(JsonNode node) {
        return MAPPER.writeValueAsString(node);
    }

    /**
     * The canonical form that a pipeline's SHA-256 is taken over: object keys sorted, no spaces
     * (section 10, Generating a batch, step 4).
     */
    public static String canonical(JsonNode node) {
        return MAPPER.writeValueAsString(sorted(node));
    }

    private static JsonNode sorted(JsonNode node) {
        if (node.isObject()) {
            List<Map.Entry<String, JsonNode>> entries = new ArrayList<>(node.properties());
            entries.sort(Map.Entry.comparingByKey());
            ObjectNode out = object();
            for (Map.Entry<String, JsonNode> e : entries) {
                out.set(e.getKey(), sorted(e.getValue()));
            }
            return out;
        }
        if (node.isArray()) {
            ArrayNode out = array();
            for (JsonNode element : node) {
                out.add(sorted(element));
            }
            return out;
        }
        return node;
    }

    /** The text of a string member, or empty when it is missing or null. */
    public static java.util.Optional<String> text(JsonNode node, String member) {
        JsonNode v = node.get(member);
        if (v == null || v.isNull() || v.isMissingNode()) {
            return java.util.Optional.empty();
        }
        if (!v.isString()) {
            throw new PipelineException("json.bad_value", "'" + member + "' must be a string");
        }
        return java.util.Optional.of(v.stringValue());
    }

    /** The text of a string member that must be there. */
    public static String requiredText(JsonNode node, String member, String what) {
        return text(node, member).orElseThrow(
                () -> new PipelineException("json.missing", what + ": '" + member + "' is missing"));
    }

    /** A boolean member, or {@code fallback} when it is missing or null. */
    public static boolean flag(JsonNode node, String member, boolean fallback) {
        JsonNode v = node.get(member);
        if (v == null || v.isNull()) {
            return fallback;
        }
        if (!v.isBoolean()) {
            throw new PipelineException("json.bad_value", "'" + member + "' must be true or false");
        }
        return v.booleanValue();
    }
}
