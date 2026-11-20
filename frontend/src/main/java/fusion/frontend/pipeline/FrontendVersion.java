package fusion.frontend.pipeline;

import java.io.IOException;
import java.io.InputStream;
import java.util.Properties;

/** The frontend's version, from the repository's one {@code VERSION} file (11.C16). */
public final class FrontendVersion {
    private FrontendVersion() {}

    private static final String VERSION = load();

    public static String current() {
        return VERSION;
    }

    private static String load() {
        try (InputStream in = FrontendVersion.class.getResourceAsStream("/fusion/frontend/version.properties")) {
            if (in == null) {
                return "0.0.0-dev";
            }
            Properties p = new Properties();
            p.load(in);
            String v = p.getProperty("version", "").trim();
            return v.isEmpty() || v.startsWith("$") ? "0.0.0-dev" : v;
        } catch (IOException e) {
            return "0.0.0-dev";
        }
    }
}
