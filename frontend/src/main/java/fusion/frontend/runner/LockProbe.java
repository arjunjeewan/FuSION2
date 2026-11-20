package fusion.frontend.runner;

import java.io.IOException;
import java.nio.channels.FileChannel;
import java.nio.channels.FileLock;
import java.nio.channels.OverlappingFileLockException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;

/**
 * Whether a run's {@code run.lock} is held (09.C9): {@code FileChannel.tryLock()}, released at once
 * when it succeeds. Java sees the {@code fcntl} lock that fusion holds on POSIX, and
 * {@code LockFileEx} on Windows. A filesystem without locks answers {@link State#UNSUPPORTED}.
 */
interface LockProbe {
    enum State { HELD, FREE, UNSUPPORTED }

    State probe(Path lockFile);

    LockProbe FILE_LOCKS = lockFile -> {
        if (!Files.exists(lockFile)) {
            return State.FREE;
        }
        try (FileChannel ch = FileChannel.open(lockFile, StandardOpenOption.READ, StandardOpenOption.WRITE)) {
            FileLock lock = ch.tryLock();
            if (lock == null) {
                return State.HELD;
            }
            lock.release();
            return State.FREE;
        } catch (OverlappingFileLockException e) {
            return State.HELD;   // held inside this JVM
        } catch (IOException | UnsupportedOperationException e) {
            return State.UNSUPPORTED;
        }
    };
}
