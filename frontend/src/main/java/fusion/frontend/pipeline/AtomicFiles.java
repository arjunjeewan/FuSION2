package fusion.frontend.pipeline;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.charset.StandardCharsets;
import java.nio.file.AccessDeniedException;
import java.nio.file.FileAlreadyExistsException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.nio.file.StandardOpenOption;
import java.nio.file.attribute.FileAttribute;
import java.nio.file.attribute.PosixFilePermission;
import java.nio.file.attribute.PosixFilePermissions;
import java.util.Set;

/**
 * Writes by atomic replace (section 10, Sessions: Writing): the bytes go to a temporary file in the
 * same directory, are forced to disk, and the file is renamed over the old one, so a crash leaves
 * the old file or the new one, never half of one. The data directory's files are owner-only on
 * POSIX (section 10, Security model). FuSION 1 rewrote its session array in place
 * ({@code Linux/frontend/src/UserInput.java:398-399}).
 */
public final class AtomicFiles {
    private AtomicFiles() {}

    private static final Set<PosixFilePermission> OWNER_FILE = PosixFilePermissions.fromString("rw-------");
    private static final Set<PosixFilePermission> OWNER_DIR = PosixFilePermissions.fromString("rwx------");

    /** Retries of the rename when a virus scanner holds the file on Windows: 5, from 50 ms, doubling. */
    static final int RENAME_RETRIES = 5;

    /**
     * Whether writes are forced to disk before the rename. Always in the application; the frontend's
     * own tests turn it off with {@code -Dfusion.files.fsync=false}, since a synced write takes 40 to
     * 65 ms on the shared workstation's disk, and the rename alone already keeps files whole.
     */
    static final boolean SYNC = Boolean.parseBoolean(System.getProperty("fusion.files.fsync", "true"));

    public static void write(Path target, String text, boolean ownerOnly) throws IOException {
        write(target, text.getBytes(StandardCharsets.UTF_8), ownerOnly);
    }

    public static void write(Path target, byte[] bytes, boolean ownerOnly) throws IOException {
        write(target, bytes, ownerOnly, SYNC);
    }

    static void write(Path target, byte[] bytes, boolean ownerOnly, boolean sync) throws IOException {
        Path dir = target.toAbsolutePath().getParent();
        ensureDirectory(dir, ownerOnly);
        Path tmp = ownerOnly && posix(dir)
                ? Files.createTempFile(dir, "." + target.getFileName() + ".", ".tmp",
                                       PosixFilePermissions.asFileAttribute(OWNER_FILE))
                : Files.createTempFile(dir, "." + target.getFileName() + ".", ".tmp");
        try {
            try (FileChannel ch = FileChannel.open(tmp, StandardOpenOption.WRITE,
                                                   StandardOpenOption.TRUNCATE_EXISTING)) {
                ByteBuffer buffer = ByteBuffer.wrap(bytes);
                while (buffer.hasRemaining()) {
                    ch.write(buffer);
                }
                if (sync) {
                    ch.force(true);
                }
            }
            move(tmp, target);
        } finally {
            Files.deleteIfExists(tmp);
        }
    }

    /** Creates the directory and its parents; the directory itself owner-only on POSIX. */
    public static void ensureDirectory(Path dir, boolean ownerOnly) throws IOException {
        if (Files.isDirectory(dir)) {
            return;
        }
        Path parent = dir.toAbsolutePath().getParent();
        if (parent != null) {
            Files.createDirectories(parent);
        }
        try {
            if (ownerOnly && posix(dir)) {
                FileAttribute<Set<PosixFilePermission>> attr = PosixFilePermissions.asFileAttribute(OWNER_DIR);
                Files.createDirectory(dir, attr);
            } else {
                Files.createDirectory(dir);
            }
        } catch (FileAlreadyExistsException e) {
            if (!Files.isDirectory(dir)) {
                throw e;
            }
        }
    }

    private static void move(Path from, Path to) throws IOException {
        long wait = 50;
        for (int attempt = 0; ; attempt++) {
            try {
                Files.move(from, to, StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING);
                return;
            } catch (AccessDeniedException e) {
                if (attempt >= RENAME_RETRIES) {
                    throw e;
                }
                try {
                    Thread.sleep(wait);
                } catch (InterruptedException ie) {
                    Thread.currentThread().interrupt();
                    throw e;
                }
                wait *= 2;
            }
        }
    }

    private static boolean posix(Path anyPath) {
        return anyPath.getFileSystem().supportedFileAttributeViews().contains("posix");
    }
}
