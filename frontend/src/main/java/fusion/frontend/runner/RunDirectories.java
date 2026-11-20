package fusion.frontend.runner;

import java.io.IOException;
import java.nio.file.FileAlreadyExistsException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.Locale;

/**
 * Run directory names (09.C1, 10.C18): {@code <label>_<YYYYMMDD-HHMMSS>} in local time, the label
 * being the session's name reduced by rule 1.26, cut to 44 characters, with a swept value appended
 * for a sweep. Each directory is created by one atomic {@code createDirectory}, trying {@code -2},
 * {@code -3} on a collision, and an existing directory is never used (S8.21). FuSION 1's runs named
 * their directory from the source, target, FSC, file and bound, and deleted it on a rerun
 * ({@code Linux/advtempscript.txt:110-130}; S8.21).
 *
 * <p>This is the Java copy of section 09's C++ rule; one golden list keeps the two in step (1.26).
 */
public final class RunDirectories {
    private RunDirectories() {}

    public static final int MAX_LABEL = 44;
    private static final DateTimeFormatter STAMP = DateTimeFormatter.ofPattern("yyyyMMdd-HHmmss", Locale.ROOT);

    /**
     * Rule 1.26: per Unicode code point, each run of code points outside {@code [A-Za-z0-9._-]}
     * becomes one {@code -}, and leading and trailing {@code -} are trimmed.
     */
    public static String reduce(String label) {
        StringBuilder sb = new StringBuilder();
        boolean replacing = false;
        for (int i = 0; i < label.length(); ) {
            int cp = label.codePointAt(i);
            i += Character.charCount(cp);
            if (allowed(cp)) {
                sb.append((char) cp);
                replacing = false;
            } else if (!replacing) {
                sb.append('-');
                replacing = true;
            }
        }
        int start = 0;
        int end = sb.length();
        while (start < end && sb.charAt(start) == '-') {
            start++;
        }
        while (end > start && sb.charAt(end - 1) == '-') {
            end--;
        }
        return sb.substring(start, end);
    }

    private static boolean allowed(int cp) {
        return (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') || (cp >= '0' && cp <= '9')
                || cp == '.' || cp == '_' || cp == '-';
    }

    /**
     * The run's label: the session's name reduced and cut to 44 characters. For a swept value the
     * input and value are appended before the reduction, and any cut comes out of the session's part,
     * so the value survives ({@code EGFR-to-CXCL8-FSC-JUN-bound-6}).
     */
    public static String label(String sessionName, String sweepSuffix) {
        String base = reduce(sessionName);
        if (sweepSuffix.isEmpty()) {
            return cut(base, MAX_LABEL);
        }
        String suffix = cut(reduce(sweepSuffix), MAX_LABEL);
        int room = MAX_LABEL - suffix.length() - 1;
        // The cut session part loses a trailing '-', so the join reads "name-bound-6", never "name--bound-6".
        String head = room <= 0 ? "" : reduce(cut(base, room));
        return head.isEmpty() ? suffix : head + "-" + suffix;
    }

    private static String cut(String s, int n) {
        return s.length() <= n ? s : s.substring(0, n);
    }

    /** {@code <label>_<YYYYMMDD-HHMMSS>}; {@code run_<timestamp>} with no label. */
    public static String name(String label, LocalDateTime when) {
        return (label.isEmpty() ? "run" : label) + "_" + STAMP.format(when);
    }

    /** Creates a new directory under {@code root}, never reusing one: {@code -2}, {@code -3} on a collision. */
    public static Path create(Path root, String label, LocalDateTime when) throws IOException {
        Files.createDirectories(root);
        String base = name(label, when);
        for (int n = 1; ; n++) {
            Path dir = root.resolve(n == 1 ? base : base + "-" + n);
            try {
                return Files.createDirectory(dir);
            } catch (FileAlreadyExistsException e) {
                if (n > 10_000) {
                    throw e;
                }
            }
        }
    }
}
