package fusion.frontend.ui;

import java.io.IOException;
import java.nio.channels.FileChannel;
import java.nio.channels.FileLock;
import java.nio.channels.OverlappingFileLockException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.util.Optional;

import fusion.frontend.pipeline.AtomicFiles;

/**
 * One instance per data directory (10.C4): an exclusive lock on {@code <data dir>/frontend.lock}.
 * A second launch writes {@code frontend.focus}, which asks the running instance to bring its window
 * forward, and exits; the running instance looks for it once per 250 ms tick and deletes it.
 * FuSION 1 had no guard: two GUIs shared the launch directory and its fixed-name files (S8.28).
 */
final class SingleInstance implements AutoCloseable {
    private final FileChannel channel;
    private final FileLock lock;
    private final Path focus;

    private SingleInstance(FileChannel channel, FileLock lock, Path dataDir) {
        this.channel = channel;
        this.lock = lock;
        this.focus = dataDir.resolve("frontend.focus");
    }

    /** The lock, or empty when another instance holds it. */
    static Optional<SingleInstance> claim(Path dataDir) throws IOException {
        AtomicFiles.ensureDirectory(dataDir, true);
        FileChannel ch = FileChannel.open(dataDir.resolve("frontend.lock"), StandardOpenOption.CREATE,
                StandardOpenOption.WRITE, StandardOpenOption.READ);
        try {
            FileLock lock = ch.tryLock();
            if (lock == null) {
                ch.close();
                return Optional.empty();
            }
            SingleInstance s = new SingleInstance(ch, lock, dataDir);
            Files.deleteIfExists(s.focus);       // a request left from before is not for this instance
            return Optional.of(s);
        } catch (OverlappingFileLockException e) {
            ch.close();
            return Optional.empty();
        }
    }

    /** Writes {@code frontend.focus} for the running instance. */
    static void askToFocus(Path dataDir) throws IOException {
        AtomicFiles.write(dataDir.resolve("frontend.focus"), "focus\n", true);
    }

    /** True once if a second launch asked for the window; the request is then deleted. */
    boolean focusRequested() {
        try {
            return Files.deleteIfExists(focus);
        } catch (IOException e) {
            return false;
        }
    }

    @Override
    public void close() throws IOException {
        lock.release();
        channel.close();
    }
}
