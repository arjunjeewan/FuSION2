package fusion.frontend.pipeline;

import java.util.List;
import java.util.Objects;

/** A file written beside the batch, such as {@code overrides/not_de.txt}, one line per entry. */
public record SideFile(String relativePath, List<String> lines) {
    public SideFile {
        Objects.requireNonNull(relativePath, "relativePath");
        lines = List.copyOf(lines);
    }

    /** The file's bytes' text: each line ended by {@code \n} (01.C19). */
    public String text() {
        StringBuilder sb = new StringBuilder();
        for (String line : lines) {
            sb.append(line).append('\n');
        }
        return sb.toString();
    }
}
