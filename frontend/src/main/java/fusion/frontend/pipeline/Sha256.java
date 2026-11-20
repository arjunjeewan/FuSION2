package fusion.frontend.pipeline;

import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.HexFormat;

/** SHA-256 digests as lower-case hex, the form {@code run.json} records (E1). */
public final class Sha256 {
    private Sha256() {}

    public static String of(byte[] bytes) {
        return HexFormat.of().formatHex(digest().digest(bytes));
    }

    public static String of(String text) {
        return of(text.getBytes(StandardCharsets.UTF_8));
    }

    /** Streams the file; 17.4 ms for a 3.08 MB table here (section 10, Checks when a session opens). */
    public static String ofFile(Path file) throws IOException {
        MessageDigest md = digest();
        byte[] buffer = new byte[64 * 1024];
        try (InputStream in = Files.newInputStream(file)) {
            int n;
            while ((n = in.read(buffer)) > 0) {
                md.update(buffer, 0, n);
            }
        }
        return HexFormat.of().formatHex(md.digest());
    }

    private static MessageDigest digest() {
        try {
            return MessageDigest.getInstance("SHA-256");
        } catch (NoSuchAlgorithmException e) {
            throw new IllegalStateException("every Java runtime has SHA-256", e);
        }
    }
}
