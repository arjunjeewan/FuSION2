package fusion.frontend.pipeline;

import java.util.ArrayList;
import java.util.List;
import java.util.regex.Pattern;

/**
 * How values are written into a batch the frontend generates (section 10, Generating a batch, step
 * 5; section 01's rules for the frontend's files, 01.C2). FuSION 1 inserted values unquoted, so a
 * path with a space became several tokens and {@code let} kept the first ({@code ScriptFile.java:75-79};
 * S8.20, S8.30).
 */
public final class BatchText {
    private BatchText() {}

    /** Section 01's named form of a duration: a whole number with a unit (01.C1). */
    public static final Pattern DURATION = Pattern.compile("[0-9]+(ms|s|min|h|d)");

    /**
     * One value as a batch token: a value with a space, a tab or a {@code "}, or an empty value, is
     * double-quoted, with {@code "} written {@code ""}; every {@code $} is written {@code $$}, so a
     * user's path never expands a variable; a control character is refused, since a batch value
     * cannot hold a line break. Backslashes and non-ASCII characters are ordinary.
     */
    public static String token(String raw) {
        refuseControl(raw);
        String escaped = raw.replace("$", "$$");
        if (raw.isEmpty() || raw.indexOf(' ') >= 0 || raw.indexOf('\t') >= 0 || raw.indexOf('"') >= 0) {
            return '"' + escaped.replace("\"", "\"\"") + '"';
        }
        return escaped;
    }

    /**
     * A value of a session, as the token that a {@code let} line or an argument holds. A list is its
     * items joined by commas, each item a token of its own, and an item that holds a comma is
     * refused; an installation path is written through {@code $FUSION_HOME} (01.C18).
     */
    public static String value(InputValue value) {
        return switch (value) {
            case InputValue.Text t -> token(t.value());
            case InputValue.Int i -> Long.toString(i.value());
            case InputValue.Real r -> InputValue.formatReal(r.value());
            case InputValue.PathValue p -> {
                if (!p.value().isAbsolute()) {
                    throw new PipelineException("generate.relative_path",
                            "a path in a batch must be absolute: " + p.value());
                }
                yield token(p.value().toString());
            }
            case InputValue.InstallPath i -> installPath(i.relative());
            case InputValue.GeneId g -> token(g.value());
            case InputValue.Choice c -> token(c.value());
            case InputValue.Flag f -> f.value() ? "true" : "false";
            case InputValue.Items l -> {
                List<String> items = new ArrayList<>();
                for (InputValue item : l.items()) {
                    if (item.text().indexOf(',') >= 0) {
                        throw new PipelineException("generate.comma_in_item",
                                "a list item cannot hold a comma: " + item.text());
                    }
                    items.add(value(item));
                }
                yield String.join(",", items);
            }
            case InputValue.Sweep s -> throw new PipelineException("generate.sweep",
                    "a swept value is written by a sweep line, or fixed for a run");
            case InputValue.Unset u -> throw new PipelineException("generate.unset", "an unset value is not written");
        };
    }

    /** {@code $FUSION_HOME/<relative>}, with {@code /} separators; quoted as a whole when needed. */
    static String installPath(String relative) {
        String rel = relative.replace('\\', '/');
        while (rel.startsWith("/")) {
            rel = rel.substring(1);
        }
        refuseControl(rel);
        String escaped = rel.replace("$", "$$");
        boolean quote = rel.indexOf(' ') >= 0 || rel.indexOf('\t') >= 0 || rel.indexOf('"') >= 0;
        String body = "$FUSION_HOME/" + (quote ? escaped.replace("\"", "\"\"") : escaped);
        return quote ? '"' + body + '"' : body;
    }

    static void refuseControl(String raw) {
        for (int i = 0; i < raw.length(); ) {
            int cp = raw.codePointAt(i);
            if (isControl(cp)) {
                throw new PipelineException("generate.control_character",
                        "a batch value cannot hold a control character (U+"
                                + String.format(java.util.Locale.ROOT, "%04X", cp) + ")");
            }
            i += Character.charCount(cp);
        }
    }

    /**
     * Control characters other than the tab, which a quoted value may hold, and the two Unicode line
     * breaks.
     */
    public static boolean isControl(int codePoint) {
        if (codePoint == '\t') {
            return false;
        }
        int type = Character.getType(codePoint);
        return type == Character.CONTROL || type == Character.LINE_SEPARATOR || type == Character.PARAGRAPH_SEPARATOR;
    }
}
