package fusion.frontend.pipeline;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.attribute.PosixFilePermissions;
import java.util.List;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.condition.DisabledOnOs;
import org.junit.jupiter.api.condition.OS;
import org.junit.jupiter.api.io.TempDir;

/** Writes by atomic replace (section 10, Sessions: Writing; 10.C8). */
class AtomicFilesTest {
    @TempDir
    Path dir;

    @Test
    void aSyncedWriteReplacesTheFileWholeAndLeavesNoTemporaryFile() throws Exception {
        Path f = dir.resolve("sub/record.json");
        AtomicFiles.write(f, "{\"version\": 1}\n".getBytes(StandardCharsets.UTF_8), true, true);
        AtomicFiles.write(f, "{\"version\": 2}\n".getBytes(StandardCharsets.UTF_8), true, true);
        assertEquals("{\"version\": 2}\n", Files.readString(f));
        try (var files = Files.list(f.getParent())) {
            assertEquals(List.of(f), files.toList());
        }
    }

    @Test
    @DisabledOnOs(OS.WINDOWS)
    void ownerOnlyFilesAndFolders() throws Exception {
        Path f = dir.resolve("data/accounts.json");
        AtomicFiles.write(f, "x", true);
        assertEquals("rw-------", PosixFilePermissions.toString(Files.getPosixFilePermissions(f)));
        assertEquals("rwx------", PosixFilePermissions.toString(Files.getPosixFilePermissions(f.getParent())));
        Path open = dir.resolve("results/stop");
        AtomicFiles.write(open, "x", false);
        assertTrue(Files.isReadable(open));
    }
}
