package fusion.frontend.ui;

import java.nio.file.Path;
import java.util.Locale;
import java.util.Map;

/**
 * Where the frontend keeps its data and puts runs by default (section 10, Settings): the
 * {@code fusion.data-dir} and {@code fusion.results-dir} settings, as system properties or the
 * environment variables {@code FUSION_DATA_DIR} and {@code FUSION_RESULTS_DIR}, else the platform's
 * per-user folders. The default results location lies outside the folders that cloud services sync.
 */
final class DataDirectory {
    private DataDirectory() {}

    static Path dataDir(Map<String, String> env, String os, Path home) {
        String set = System.getProperty("fusion.data-dir", env.get("FUSION_DATA_DIR"));
        if (set != null && !set.isBlank()) {
            return Path.of(set);
        }
        String name = os.toLowerCase(Locale.ROOT);
        if (name.startsWith("windows")) {
            String local = env.getOrDefault("LOCALAPPDATA", home.resolve("AppData/Local").toString());
            return Path.of(local, "FuSION", "frontend");
        }
        if (name.startsWith("mac")) {
            return home.resolve("Library/Application Support/FuSION/frontend");
        }
        String xdg = env.get("XDG_DATA_HOME");
        return (xdg != null && !xdg.isBlank() ? Path.of(xdg) : home.resolve(".local/share")).resolve("fusion/frontend");
    }

    static Path resultsDir(Map<String, String> env, Path home) {
        String set = System.getProperty("fusion.results-dir", env.get("FUSION_RESULTS_DIR"));
        return set != null && !set.isBlank() ? Path.of(set) : home.resolve("FuSION").resolve("runs");
    }
}
