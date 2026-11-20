package fusion.frontend.runner;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Deque;
import java.util.List;

/**
 * One first-in, first-out queue for all of an instance's runs, counted in cores (10.C20): a run
 * needs one core per FSC case of its solve (06.C4), 2 for {@code in} and {@code out}, 1 for
 * {@code free} and 1 for a run without a solve, and starts when that many are free. A run that needs
 * more than the whole limit starts alone. FuSION 1 refused nothing: a second Submit shared the
 * launch directory with the first ({@code Gui4Panel.java:141-195}; S8.28).
 */
public final class RunQueue {
    private record Entry(RunId id, int cores) {}

    private final int limit;
    private final Deque<Entry> queue = new ArrayDeque<>();

    public RunQueue(int limit) {
        if (limit < 1) {
            throw new IllegalArgumentException("the core limit is at least 1");
        }
        this.limit = limit;
    }

    /** {@code max(1, min(⌊logical CPUs / 2⌋, ⌊physical memory / 3 GiB⌋))}: 01.C4's --jobs auto. */
    public static int defaultLimit(int logicalCpus, long physicalMemoryBytes) {
        long byMemory = physicalMemoryBytes / (3L << 30);
        return (int) Math.max(1, Math.min(logicalCpus / 2, byMemory));
    }

    public int limit() {
        return limit;
    }

    public synchronized void add(RunId id, int cores) {
        queue.addLast(new Entry(id, Math.max(1, cores)));
    }

    /** Removes a queued run; false if it was not queued. */
    public synchronized boolean cancel(RunId id) {
        return queue.removeIf(e -> e.id().equals(id));
    }

    public synchronized boolean contains(RunId id) {
        return queue.stream().anyMatch(e -> e.id().equals(id));
    }

    /** The runs to start now, in order, given the cores in use; strictly first in, first out. */
    public synchronized List<RunId> startable(int coresInUse) {
        List<RunId> out = new ArrayList<>();
        int used = coresInUse;
        while (!queue.isEmpty()) {
            Entry head = queue.peekFirst();
            boolean fits = used + head.cores() <= limit;
            boolean alone = head.cores() > limit && used == 0;
            if (!fits && !alone) {
                break;
            }
            queue.pollFirst();
            out.add(head.id());
            used += head.cores();
        }
        return out;
    }
}
