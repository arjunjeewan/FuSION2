package fusion.frontend.runner;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.time.LocalDateTime;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.UUID;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

import fusion.frontend.pipeline.Binding;
import fusion.frontend.pipeline.Block;
import fusion.frontend.pipeline.InputValue;
import fusion.frontend.pipeline.PipelineDocument;
import fusion.frontend.pipeline.ResolvedPipeline;
import fusion.frontend.pipeline.TestFixtures;

/** Section 10's runner test hooks that need no process: names, exits, tails, the queue and cores. */
class RunnerUnitTest {
    @TempDir
    Path tmp;

    // ---- run directory names (09.C1, 1.26, 10.C18) ----

    /** The Java copy of rule 1.26 against the golden table shared with section 09's C++ copy. */
    @Test
    void labelsMatchTheSharedGoldenTable() {
        String table = TestFixtures.resource("/fusion/frontend/runner/run-dir-labels.tsv");
        int rows = 0;
        for (String line : table.split("\n")) {
            if (line.startsWith("#") || line.isEmpty()) {
                continue;
            }
            String[] cols = line.split("\t", -1);
            assertEquals(2, cols.length, line);
            assertEquals(cols[1], RunDirectories.label(cols[0], ""), "label of '" + cols[0] + "'");
            rows++;
        }
        assertEquals(14, rows);
        assertEquals("tab-here", RunDirectories.reduce("tab\there"));
    }

    @Test
    void sweptValuesSurviveTheCut() {
        assertEquals("EGFR-to-CXCL8-FSC-JUN-bound-6", RunDirectories.label("EGFR to CXCL8, FSC JUN", "bound-6"));
        String long44 = RunDirectories.label("A very long session name that goes on beyond forty-four characters",
                "bound-10");
        assertEquals("A-very-long-session-name-that-goes-bound-10", long44);
        assertTrue(long44.length() <= RunDirectories.MAX_LABEL);
        assertEquals("bound-6", RunDirectories.label("日本語", "bound-6"));
    }

    @Test
    void directoryNamesAreNewAndNeverReused() throws IOException {
        LocalDateTime when = LocalDateTime.of(2026, 11, 12, 18, 14, 6);
        assertEquals("EGFR-to-CXCL8-FSC-JUN_20261112-181406", RunDirectories.name("EGFR-to-CXCL8-FSC-JUN", when));
        assertEquals("run_20261112-181406", RunDirectories.name("", when));
        Path a = RunDirectories.create(tmp.resolve("runs"), "x", when);
        Path b = RunDirectories.create(tmp.resolve("runs"), "x", when);
        Path c = RunDirectories.create(tmp.resolve("runs"), "x", when);
        assertEquals(List.of("x_20261112-181406", "x_20261112-181406-2", "x_20261112-181406-3"),
                List.of(a.getFileName().toString(), b.getFileName().toString(), c.getFileName().toString()));
    }

    // ---- the exit mapping (section 10, Errors; 01.C14) ----

    @Test
    void exitStatusesMapToStates() {
        assertEquals(RunState.FINISHED, ExitMapping.state(0, false, Optional.empty()));
        assertEquals(RunState.FAILED, ExitMapping.state(1, false, Optional.empty()));
        assertEquals(RunState.FAILED, ExitMapping.state(2, false, Optional.empty()));
        assertEquals(RunState.STOPPED, ExitMapping.state(3, false, Optional.empty()));
        assertEquals(RunState.STOPPED, ExitMapping.state(130, false, Optional.empty()));
        assertEquals(RunState.KILLED, ExitMapping.state(137, true, Optional.empty()));
        for (int signalled : new int[] {134, 137, 139, 143}) {
            assertEquals(RunState.CRASHED, ExitMapping.state(signalled, false, Optional.empty()));
        }
        assertEquals(RunState.CRASHED, ExitMapping.state(-1073741819, false, Optional.empty()));   // 0xC0000005
        // A final record decides an odd status: the run ended, then its process was ended.
        assertEquals(RunState.FINISHED, ExitMapping.state(137, false, Optional.of("ok")));
        assertTrue(ExitMapping.describe(137, false).contains("signal 9"));
        assertTrue(ExitMapping.describe(139, false).contains("segmentation fault"));
        assertEquals("Windows status 0xC0000005", ExitMapping.describe(-1073741819, true));
        assertEquals("exit status 1", ExitMapping.describe(1, false));
    }

    // ---- EventTail (A2) ----

    private static void append(Path f, String text) throws IOException {
        Files.writeString(f, text, StandardCharsets.UTF_8, StandardOpenOption.CREATE, StandardOpenOption.APPEND);
    }

    @Test
    void aLineSplitAcrossTwoReadsCrlfAndALateFile() throws IOException {
        Path f = tmp.resolve("events.jsonl");
        EventTail tail = new EventTail(f);
        assertEquals(List.of(), tail.poll());                        // the file appears late
        append(f, "{\"seq\":1}\n{\"se");
        assertEquals(List.of(new EventTail.Line(0, "{\"seq\":1}")), tail.poll());
        append(f, "q\":2}\r\n{\"seq\":3,\"t\":\"é\"}\n");
        assertEquals(List.of(new EventTail.Line(10, "{\"seq\":2}"), new EventTail.Line(21, "{\"seq\":3,\"t\":\"é\"}")),
                tail.poll());
        assertEquals(List.of(), tail.poll());
        assertEquals(Files.size(f), tail.offset());
    }

    @Test
    void aFileThatShrinksIsAnError() throws IOException {
        Path f = tmp.resolve("run.log");
        EventTail tail = new EventTail(f);
        append(f, "one\ntwo\n");
        assertEquals(2, tail.poll().size());
        Files.writeString(f, "o\n");
        assertThrows(IOException.class, tail::poll);
    }

    // ---- the queue (10.C20) ----

    private static RunId id(int n) {
        return new RunId(new UUID(0, n));
    }

    @Test
    void theQueueIsFirstInFirstOutCountedInCores() {
        RunQueue q = new RunQueue(4);
        q.add(id(1), 2);
        q.add(id(2), 2);
        q.add(id(3), 1);
        assertEquals(List.of(id(1), id(2)), q.startable(0));
        assertEquals(List.of(), q.startable(4));
        assertEquals(List.of(id(3)), q.startable(2));
        // Strictly in order: a small run waits behind a large one.
        q.add(id(4), 3);
        q.add(id(5), 1);
        assertEquals(List.of(), q.startable(2));
        assertEquals(List.of(id(4), id(5)), q.startable(0));
    }

    @Test
    void aRunLargerThanTheLimitStartsAloneAndAQueuedRunCanBeCancelled() {
        RunQueue q = new RunQueue(2);
        q.add(id(1), 3);
        q.add(id(2), 1);
        assertEquals(List.of(), q.startable(1));
        assertEquals(List.of(id(1)), q.startable(0));
        assertTrue(q.cancel(id(2)));
        assertFalse(q.cancel(id(2)));
        assertEquals(List.of(), q.startable(0));
    }

    @Test
    void theDefaultLimitIsHalfTheCpusAndThreeGibPerCore() {
        assertEquals(18, RunQueue.defaultLimit(36, 124L << 30));               // this machine
        assertEquals(4, RunQueue.defaultLimit(8, (long) (15.5 * (1L << 30))));   // an 8-thread laptop
        assertEquals(1, RunQueue.defaultLimit(1, 1L << 30));
    }

    /** One core per FSC case of the solve (06.C4): 2 for in and out, 1 for free, cases when given. */
    @Test
    void coresFollowTheSolvesCases() {
        PipelineDocument std = TestFixtures.standard();
        Map<String, InputValue> v = TestFixtures.egfSession();
        assertEquals(2, DefaultRunService.cores(ResolvedPipeline.of(std, v, List.of(), TestFixtures.catalog())));
        v.put("fsc", InputValue.UNSET);
        assertEquals(1, DefaultRunService.cores(ResolvedPipeline.of(std, v, List.of(), TestFixtures.catalog())));
        PipelineDocument three = new PipelineDocument(1, "t@1", std.inputs(), std.blocks().stream()
                .map(b -> b.id().equals("solve") ? withCases(b) : b).toList());
        assertEquals(3, DefaultRunService.cores(ResolvedPipeline.of(three, TestFixtures.egfSession(), List.of(),
                TestFixtures.catalog())));
    }

    private static Block withCases(Block b) {
        Map<String, Binding> args = new java.util.LinkedHashMap<>(b.args());
        args.put("cases", new Binding.Literal(new InputValue.Items(List.of(TestFixtures.text("in"),
                TestFixtures.text("out"), TestFixtures.text("free")))));
        return new Block(b.id(), b.command(), args, b.when(), b.x(), b.y());
    }
}
