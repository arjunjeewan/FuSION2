package fusion.frontend.runner;

import static fusion.frontend.runner.RunnerFixture.await;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Duration;
import java.time.Instant;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.OptionalInt;
import java.util.OptionalLong;
import java.util.UUID;

import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.condition.DisabledOnOs;
import org.junit.jupiter.api.condition.OS;
import org.junit.jupiter.api.io.TempDir;

import fusion.frontend.pipeline.InputValue;
import fusion.frontend.pipeline.Json;
import fusion.frontend.pipeline.PipelineProblem;
import fusion.frontend.pipeline.Severity;
import fusion.frontend.pipeline.TestFixtures;
import fusion.frontend.sessions.Session;
import fusion.frontend.sessions.SessionException;
import tools.jackson.databind.JsonNode;

/**
 * Section 10's process tests, with {@link FakeFusion} in place of {@code fusion} (section 12,
 * Frontend contracts): following a run, Stop within the grace period and 09.C7's forced exit, Kill
 * of a run with a child, the exit mapping, reattachment through {@code run.lock}, the queue and
 * Resume. POSIX only for now: the fake's launcher is a shell script.
 */
@DisabledOnOs(OS.WINDOWS)
class RunServiceProcessTest {
    @TempDir
    Path tmp;

    private RunnerFixture f;

    @BeforeEach
    void setUp() throws Exception {
        f = new RunnerFixture(tmp);
    }

    @AfterEach
    void tearDown() {
        f.close();
    }

    private static final String WAIT_LONG = "{\"after_ms\": 15000, \"log\": \"never reached\"}";

    @Test
    void aRunIsFollowedThroughItsEventsAndLogToItsEnd() throws Exception {
        Path fake = f.fake(
                "{\"after_ms\": 0, \"event\": {\"type\": \"step.started\", \"step\": \"cudf\", \"cmd\": \"cudf\"}}",
                "{\"after_ms\": 0, \"event\": {\"type\": \"step.finished\", \"step\": \"cudf\", \"status\": \"ok\"}}",
                "{\"after_ms\": 0, \"event\": {\"type\": \"step.started\", \"step\": \"pathz3\", \"cmd\": \"pathz3\"}}",
                "{\"after_ms\": 0, \"event\": {\"type\": \"solve.check.started\", \"step\": \"pathz3\", \"case\": \"in\", \"check\": 1, \"n\": 0, \"e\": 4}}",
                "{\"after_ms\": 0, \"event\": {\"type\": \"solve.check\", \"step\": \"pathz3\", \"case\": \"in\", \"outcome\": \"unsat\"}}",
                "{\"after_ms\": 0, \"event\": {\"type\": \"solve.check\", \"step\": \"pathz3\", \"case\": \"out\", \"outcome\": \"unknown\"}}",
                "{\"after_ms\": 0, \"event\": {\"type\": \"solve.point\", \"step\": \"pathz3\", \"case\": \"in\", \"n\": 1, \"e\": 0}}",
                "{\"after_ms\": 0, \"event\": {\"type\": \"solve.case.finished\", \"step\": \"pathz3\", \"case\": \"in\", \"status\": \"complete\"}}",
                "{\"after_ms\": 0, \"log\": \"info  pathz3: with FSC (FSC13): 1 Pareto point, certified\"}",
                "{\"after_ms\": 50, \"exit\": 0}");
        DefaultRunService runs = f.start(fake, 4, Duration.ofMillis(300));
        Session s = f.toySession("Toy hsa1 to hsa22, FSC hsa13");
        List<RunId> ids = runs.submit(f.arjun.id(), s.id(), SubmitRequest.QUICK);
        assertEquals(1, ids.size());
        RunId id = ids.get(0);
        f.awaitState(runs, id, RunState.FINISHED);

        RunView v = runs.view(f.arjun.id(), id).orElseThrow();
        RunRecord r = v.record();
        assertEquals(2, r.cores());                                  // in and out (06.C4)
        assertEquals(OptionalInt.of(0), r.osExitStatus());
        assertTrue(r.fusionRunId().orElseThrow().startsWith("r-"));
        Path dir = r.runDir().orElseThrow();
        assertTrue(dir.getFileName().toString().startsWith("Toy-hsa1-to-hsa22-FSC-hsa13_"), dir.toString());
        assertEquals(f.resultsDir, dir.getParent());
        assertTrue(Files.readString(dir.resolve("run.batch")).contains("rgx file="));
        // The session's id reaches run.json through --tag (10.C18).
        JsonNode runJson = Json.parse(Files.readString(dir.resolve("run.json")), "run.json");
        assertEquals(s.id().toString(), runJson.path("labels").path("session").asString());
        assertEquals(id.toString(), runJson.path("labels").path("frontend_run").asString());
        // Progress from the events (A2): the stage by its catalog label and its block.
        assertEquals(Optional.of("Constraint solving (Z3)"), v.progress().stage());
        assertEquals(Optional.of("solve"), v.progress().block());
        assertEquals(2, v.progress().checks());
        assertEquals(1, v.progress().unknowns());
        assertEquals(Map.of("in", 1), v.progress().pointsByCase());
        assertEquals(Map.of("in", "complete"), v.caseStatuses());
        // What the subscriber saw, once per tick (10.C5).
        await("the final status message", () -> f.messages.stream().anyMatch(m -> m instanceof RunMessage.Status st
                && st.state() == RunState.FINISHED));
        assertTrue(f.messages.stream().anyMatch(m -> m instanceof RunMessage.Status st && st.state() == RunState.RUNNING));
        assertEquals(10, f.messages.stream().filter(m -> m instanceof RunMessage.Event).count());
        String log = String.join("", f.messages.stream().filter(m -> m instanceof RunMessage.Log)
                .map(m -> ((RunMessage.Log) m).text()).toList());
        assertTrue(log.contains("1 Pareto point, certified"), log);
        assertTrue(f.deliveries.size() < f.messages.size(), "messages are batched per tick");
        // The run's directory is listed with its session, newest last.
        assertEquals(List.of(id), runs.runsOf(f.arjun.id(), s.id()).stream().map(RunRecord::id).toList());
        assertFalse(runs.busy(s.id()));
    }

    @Test
    void stopIsAFileAndTheRunEndsStoppedWithinItsGrace() throws Exception {
        Path fake = f.fake("{\"after_ms\": 0, \"on_stop\": {\"action\": \"honour\", \"within_ms\": 100}}", WAIT_LONG);
        DefaultRunService runs = f.start(fake, 4, Duration.ofMillis(300));
        Session s = f.toySession("stop me");
        RunId id = runs.submit(f.arjun.id(), s.id(), SubmitRequest.QUICK).get(0);
        f.awaitState(runs, id, RunState.RUNNING);
        assertTrue(runs.busy(s.id()));
        assertThrows(SessionException.SessionBusy.class, () -> f.sessions.delete(f.arjun.id(), s.id()));
        runs.stop(f.arjun.id(), id);
        assertEquals(RunState.STOPPING, f.state(runs, id));
        Path dir = runs.view(f.arjun.id(), id).orElseThrow().record().runDir().orElseThrow();
        assertTrue(Files.readString(dir.resolve("stop")).startsWith("requested by arjun from the frontend at "));
        f.awaitState(runs, id, RunState.STOPPED);
        assertEquals(OptionalInt.of(3), runs.view(f.arjun.id(), id).orElseThrow().record().osExitStatus());
    }

    @Test
    void fusionsOwnForcedExitEndsAStopThatAStepIgnores() throws Exception {
        Path fake = f.fake("{\"after_ms\": 0, \"on_stop\": {\"action\": \"ignore\", \"forced_after_ms\": 200}}", WAIT_LONG);
        DefaultRunService runs = f.start(fake, 4, Duration.ofSeconds(60));
        RunId id = runs.submit(f.arjun.id(), f.toySession("forced").id(), SubmitRequest.QUICK).get(0);
        f.awaitState(runs, id, RunState.RUNNING);
        runs.stop(f.arjun.id(), id);
        f.awaitState(runs, id, RunState.STOPPED);
        assertFalse(runs.killOffered(f.arjun.id(), id));
    }

    @Test
    void killEndsTheProcessTreeOnceTheGraceHasPassed() throws Exception {
        Path fake = f.fake("{\"after_ms\": 0, \"spawn_child\": true}",
                "{\"after_ms\": 0, \"on_stop\": {\"action\": \"ignore\"}}", WAIT_LONG);
        DefaultRunService runs = f.start(fake, 4, Duration.ofMillis(300));
        RunId id = runs.submit(f.arjun.id(), f.toySession("stuck").id(), SubmitRequest.QUICK).get(0);
        f.awaitState(runs, id, RunState.RUNNING);
        Path dir = runs.view(f.arjun.id(), id).orElseThrow().record().runDir().orElseThrow();
        await("the child", () -> Files.exists(dir.resolve("child.pid")));
        long child = Long.parseLong(Files.readString(dir.resolve("child.pid")).strip());
        assertThrows(RunnerException.NotKillable.class, () -> runs.kill(f.arjun.id(), id));   // not stopped yet
        runs.stop(f.arjun.id(), id);
        assertFalse(runs.killOffered(f.arjun.id(), id));
        await("the grace to pass", () -> runs.killOffered(f.arjun.id(), id));
        runs.kill(f.arjun.id(), id);
        f.awaitState(runs, id, RunState.KILLED);
        await("the child to end", () -> ProcessHandle.of(child).map(ph -> !ph.isAlive()).orElse(true));
    }

    @Test
    void aFailureAndACrashAreToldApart() throws Exception {
        DefaultRunService runs = f.start(f.fake("{\"after_ms\": 0, \"exit\": 1}"), 4, Duration.ofMillis(300));
        RunId failed = runs.submit(f.arjun.id(), f.toySession("fails").id(), SubmitRequest.QUICK).get(0);
        f.awaitState(runs, failed, RunState.FAILED);
        assertEquals("fake.failed", runs.view(f.arjun.id(), failed).orElseThrow().failure().orElseThrow()
                .path("code").asString());
        runs.close();
        DefaultRunService crashing = f.start(f.fake("{\"after_ms\": 0, \"crash\": true}"), 4, Duration.ofMillis(300));
        RunId crashed = crashing.submit(f.arjun.id(), f.toySession("crashes").id(), SubmitRequest.QUICK).get(0);
        f.awaitState(crashing, crashed, RunState.CRASHED);
        RunView v = crashing.view(f.arjun.id(), crashed).orElseThrow();
        assertEquals(OptionalInt.of(137), v.record().osExitStatus());
        assertTrue(v.failure().orElseThrow().path("message").asString().contains("signal 9"));
    }

    @Test
    void aProcessThatCannotStartIsNotStarted() throws Exception {
        DefaultRunService runs = f.start(tmp.resolve("no-such-fusion"), 4, Duration.ofMillis(300));
        RunId id = runs.submit(f.arjun.id(), f.toySession("nothing").id(), SubmitRequest.QUICK).get(0);
        f.awaitState(runs, id, RunState.NOT_STARTED);
        assertTrue(runs.view(f.arjun.id(), id).orElseThrow().failure().orElseThrow().path("message").asString()
                .contains("no-such-fusion"));
    }

    /** Runs outlive the frontend: a new runner reattaches through run.lock (10.C16, 09.C9). */
    @Test
    void aRestartedRunnerReattachesToARunningRun() throws Exception {
        Path fake = f.fake("{\"after_ms\": 0, \"log\": \"before the restart\"}",
                "{\"after_ms\": 0, \"wait_for\": \"go\"}", "{\"after_ms\": 0, \"exit\": 0}");
        DefaultRunService first = f.start(fake, 4, Duration.ofMillis(300));
        Session s = f.toySession("outlives");
        RunId id = first.submit(f.arjun.id(), s.id(), SubmitRequest.QUICK).get(0);
        f.awaitState(first, id, RunState.RUNNING);
        Path dir = first.view(f.arjun.id(), id).orElseThrow().record().runDir().orElseThrow();
        await("the run's first log line", () -> Files.exists(dir.resolve(dir.getFileName() + ".log")));
        first.close();                                     // the frontend quits; the run goes on

        f.messages.clear();
        DefaultRunService second = f.start(fake, 4, Duration.ofMillis(300));
        assertEquals(RunState.RUNNING, f.state(second, id));
        assertTrue(second.busy(s.id()));
        await("the log from offset 0", () -> f.messages.stream().anyMatch(m -> m instanceof RunMessage.Log l
                && l.from() == 0 && l.text().contains("before the restart")));
        Files.writeString(dir.resolve("go"), "");
        f.awaitState(second, id, RunState.FINISHED);
        assertEquals(OptionalInt.of(0), second.view(f.arjun.id(), id).orElseThrow().record().osExitStatus());
    }

    @Test
    void aRunWhoseLockIsFreeAfterARestartIsLost() throws Exception {
        RunRecord dead = writeRecord("dead", RunState.RUNNING, ProcessHandle.current().pid() + 1_000_000);
        DefaultRunService runs = f.start(tmp.resolve("unused"), 4, Duration.ofMillis(300));
        assertEquals(RunState.LOST, f.state(runs, dead.id()));
    }

    /** On a filesystem without locks, the process id and start time stand in (10.C16). */
    @Test
    void withoutLocksThePidAndItsStartTimeDecide() throws Exception {
        ProcessHandle me = ProcessHandle.current();
        RunRecord alive = writeRecord("alive", RunState.RUNNING, me.pid());
        RunRecord gone = writeRecord("gone", RunState.STOPPING, me.pid() + 1_000_000);
        DefaultRunService runs = f.start(tmp.resolve("unused"), 4, Duration.ofMillis(300),
                lock -> LockProbe.State.UNSUPPORTED);
        assertEquals(RunState.RUNNING, f.state(runs, alive.id()));
        assertEquals(RunState.LOST, f.state(runs, gone.id()));
    }

    /** A record and a run directory whose run.json still says running, as a crash leaves them. */
    private RunRecord writeRecord(String name, RunState state, long pid) throws Exception {
        Path dir = Files.createDirectories(f.resultsDir.resolve(name + "_20261112-181406"));
        Files.writeString(dir.resolve("run.json"), "{\"format\": \"fusion.run\", \"status\": \"running\"}");
        Files.writeString(dir.resolve("run.lock"), "held by no process\n");
        Session s = f.toySession(name);
        Optional<Instant> start = pid == ProcessHandle.current().pid() ? ProcessHandle.current().info().startInstant()
                : Optional.empty();
        RunRecord r = new RunRecord(new RunId(UUID.randomUUID()), s.id(), f.arjun.id(), PipelineChoice.DEFAULT, "x", 1,
                Optional.of(dir), Optional.empty(), OptionalInt.empty(), Optional.empty(), Optional.empty(), Instant.now(),
                Optional.of(Instant.now()), Optional.empty(), OptionalLong.of(pid), start, state, OptionalInt.empty());
        new RunStore(f.dataDir).save(new RunStore.Stored(r, name, Optional.of(f.resultsDir), Optional.empty(),
                RunStore.ResumeMode.NONE, Optional.empty(), Optional.empty(), false, Optional.empty(), List.of()));
        return r;
    }

    @Test
    void theQueueHoldsARunUntilItsCoresAreFree() throws Exception {
        Path fake = f.fake("{\"after_ms\": 0, \"wait_for\": \"go\"}", "{\"after_ms\": 0, \"exit\": 0}");
        DefaultRunService runs = f.start(fake, 2, Duration.ofMillis(300));
        RunId first = runs.submit(f.arjun.id(), f.toySession("first").id(), SubmitRequest.QUICK).get(0);
        RunId second = runs.submit(f.arjun.id(), f.toySession("second").id(), SubmitRequest.QUICK).get(0);
        f.awaitState(runs, first, RunState.RUNNING);
        Thread.sleep(200);
        assertEquals(RunState.QUEUED, f.state(runs, second));        // 2 + 2 cores > 2
        Path dir = runs.view(f.arjun.id(), first).orElseThrow().record().runDir().orElseThrow();
        Files.writeString(dir.resolve("go"), "");
        f.awaitState(runs, first, RunState.FINISHED);
        f.awaitState(runs, second, RunState.RUNNING);
        // Stop on a queued run cancels it (S8.04's Cancel cancels).
        RunId third = runs.submit(f.arjun.id(), f.toySession("third").id(), SubmitRequest.QUICK).get(0);
        runs.stop(f.arjun.id(), third);
        assertEquals(RunState.CANCELLED, f.state(runs, third));
        assertThrows(RunnerException.NotResumable.class, () -> runs.resume(f.arjun.id(), third));
        Path dir2 = runs.view(f.arjun.id(), second).orElseThrow().record().runDir().orElseThrow();
        Files.writeString(dir2.resolve("go"), "");
        f.awaitState(runs, second, RunState.FINISHED);
    }

    @Test
    void resumeContinuesAStoppedRunAndRestartsAFailedOneFromItsLastGoodStep() throws Exception {
        Path record = tmp.resolve("commands.txt");
        Path dir = Files.createDirectories(tmp.resolve("fake-resume"));
        Path script = dir.resolve("scenario.jsonl");
        Files.writeString(script, "{\"after_ms\": 0, \"exit\": 3}\n");
        Path fake = FakeFusion.launcher(dir, Map.of("FAKE_FUSION_SCRIPT", script.toString(),
                "FAKE_FUSION_RECORD", record.toString()));
        DefaultRunService runs = f.start(fake, 4, Duration.ofMillis(300));
        Session s = f.toySession("resumable");
        RunId stopped = runs.submit(f.arjun.id(), s.id(), SubmitRequest.QUICK).get(0);
        f.awaitState(runs, stopped, RunState.STOPPED);
        Path oldDir = runs.view(f.arjun.id(), stopped).orElseThrow().record().runDir().orElseThrow();
        RunId resumed = runs.resume(f.arjun.id(), stopped);
        f.awaitState(runs, resumed, RunState.STOPPED);
        RunRecord r = runs.view(f.arjun.id(), resumed).orElseThrow().record();
        assertEquals(Optional.of(stopped), r.resumedFrom());
        assertFalse(r.runDir().orElseThrow().equals(oldDir));          // never reused (S8.21)
        List<String> commands = Files.readAllLines(record);
        assertTrue(commands.get(1).startsWith("--resume " + oldDir + " --run-dir "), commands.get(1));

        Files.writeString(script, "{\"after_ms\": 0, \"exit\": 1}\n");
        RunId failed = runs.submit(f.arjun.id(), s.id(), SubmitRequest.QUICK).get(0);
        f.awaitState(runs, failed, RunState.FAILED);
        Path failedDir = runs.view(f.arjun.id(), failed).orElseThrow().record().runDir().orElseThrow();
        RunId restarted = runs.resume(f.arjun.id(), failed);
        f.awaitState(runs, restarted, RunState.FAILED);
        String restart = Files.readAllLines(record).get(3);
        assertTrue(restart.startsWith("-b ") && restart.contains(" --resume " + failedDir + " --run-dir "), restart);
    }

    @Test
    void aSweptSessionMakesOneRunPerValue() throws Exception {
        Path fake = f.fake("{\"after_ms\": 0, \"exit\": 0}");
        DefaultRunService runs = f.start(fake, 8, Duration.ofMillis(300));
        Session s = f.toySession("EGFR to CXCL8, FSC JUN");
        Session swept = f.sessions.update(f.arjun.id(), s.id(), s.version(), x -> x.withInput("bound",
                new InputValue.Sweep(List.of(TestFixtures.integer(6), TestFixtures.integer(8)))));
        List<RunId> ids = runs.submit(f.arjun.id(), swept.id(), SubmitRequest.QUICK);
        assertEquals(2, ids.size());
        for (RunId id : ids) {
            f.awaitState(runs, id, RunState.FINISHED);
        }
        List<RunRecord> records = runs.runsOf(f.arjun.id(), s.id());
        assertEquals(records.get(0).sweepGroup(), records.get(1).sweepGroup());
        assertTrue(records.get(0).sweepGroup().isPresent());
        List<String> names = records.stream().map(r -> r.runDir().orElseThrow().getFileName().toString())
                .sorted().toList();
        assertTrue(names.get(0).startsWith("EGFR-to-CXCL8-FSC-JUN-bound-6_"), names.toString());
        assertTrue(names.get(1).startsWith("EGFR-to-CXCL8-FSC-JUN-bound-8_"), names.toString());
    }

    @Test
    void submitIsRefusedWithEveryProblemAndLeavesNoRunDirectory() throws Exception {
        DefaultRunService runs = f.start(tmp.resolve("unused"), 4, Duration.ofMillis(300));
        Session s = f.toySession("refused");
        Session bad = f.sessions.update(f.arjun.id(), s.id(), s.version(),
                x -> x.withInput("up", InputValue.UNSET).withInput("target", new InputValue.GeneId("hsa99999")));
        RunnerException.ValidationFailed e = assertThrows(RunnerException.ValidationFailed.class,
                () -> runs.submit(f.arjun.id(), bad.id(), SubmitRequest.QUICK));
        assertEquals(List.of("input.required"), e.problems().stream().filter(PipelineProblem::isError)
                .map(PipelineProblem::code).toList());
        Session unknownGene = f.sessions.update(f.arjun.id(), s.id(), bad.version(), x -> x.withInput("up",
                TestFixtures.real(0.9)));
        e = assertThrows(RunnerException.ValidationFailed.class,
                () -> runs.submit(f.arjun.id(), unknownGene.id(), SubmitRequest.QUICK));
        assertEquals(List.of("input.unknown_gene"), e.problems().stream().map(PipelineProblem::code).toList());
        // fusion --check's errors, and its undefined-variable warnings, map back to blocks (S8.20, S8.30).
        Session good = f.sessions.update(f.arjun.id(), s.id(), unknownGene.version(), x -> x.withInput("target",
                new InputValue.GeneId("hsa22")));
        f.backend.check = new CheckReport(false, List.of(
                new CheckDiagnostic(Severity.ERROR, 19, OptionalInt.empty(), "network.bad_kgml", "bad file",
                        Optional.of("network.file"), Optional.empty()),
                new CheckDiagnostic(Severity.WARNING, 18, OptionalInt.empty(), "batch.undefined_variable", "$X",
                        Optional.empty(), Optional.empty())));
        e = assertThrows(RunnerException.ValidationFailed.class,
                () -> runs.submit(f.arjun.id(), good.id(), SubmitRequest.QUICK));
        List<PipelineProblem> p = e.problems().stream().filter(PipelineProblem::isError).toList();
        assertEquals(List.of("network.bad_kgml", "batch.undefined_variable"), p.stream().map(PipelineProblem::code).toList());
        assertEquals(Optional.of("merged_network"), p.get(0).block());
        assertEquals(Optional.of("file"), p.get(0).argument());
        assertEquals(Optional.of("merged_graph"), p.get(0).input());
        assertFalse(Files.exists(f.resultsDir));
        // A backend of another version is refused (11.C16).
        f.backend.check = new CheckReport(true, List.of());
        f.backend.version = "0.2.0";
        assertThrows(RunnerException.BackendUnavailable.class, () -> runs.submit(f.arjun.id(), good.id(), SubmitRequest.QUICK));
        assertEquals(List.of(), runs.runsOf(f.arjun.id(), s.id()));
    }

    @Test
    void checkWarningsAreShownWithTheRunAndDoNotBlockIt() throws Exception {
        // fusion --check, --commands, --version and the labels query answered by FakeFusion itself.
        Path dir = Files.createTempDirectory(tmp, "fake");
        Path script = Files.write(dir.resolve("scenario.jsonl"), List.of("{\"after_ms\": 0, \"exit\": 0}"));
        Path check = Files.writeString(dir.resolve("check.json"), """
                {"ok": true, "batch": "run.batch", "points": 1, "diagnostics": [
                 {"severity": "note", "line": 3, "column": 1, "point": null, "code": "batch.note", "command": null,
                  "setting": null, "path": null, "message": "a note on the values file"},
                 {"severity": "warning", "line": 22, "column": 0, "point": null, "code": "deps.font_missing",
                  "command": "plot_pareto", "setting": null, "path": null,
                  "message": "plot_pareto: the font Liberation Sans is missing; gnuplot draws with its default"}],
                 "features": []}""");
        Path catalog = Files.writeString(dir.resolve("catalog.json"), TestFixtures.catalogText());
        Path labels = Files.writeString(dir.resolve("labels.json"),
                "{\"labels\": [[\"hsa1\", \"SRC1\"], [\"hsa13\", \"FSC13\"], [\"hsa22\", \"TGT22\"]]}");
        Path fake = FakeFusion.launcher(dir, Map.of("FAKE_FUSION_SCRIPT", script.toString(), "FAKE_FUSION_CHECK",
                check.toString(), "FAKE_FUSION_CATALOG", catalog.toString(), "FAKE_FUSION_LABELS", labels.toString(),
                "FAKE_FUSION_HOME", tmp.resolve("home").toString()));
        ProcessBackendClient client = new ProcessBackendClient(fake, Duration.ofSeconds(20), 4);
        DefaultRunService runs = f.start(fake, client, 4, Duration.ofMillis(300), LockProbe.FILE_LOCKS);
        Session s = f.toySession("warned");

        // Submit goes ahead, and the run's view shows the warnings, tied to their input and block.
        RunId id = runs.submit(f.arjun.id(), s.id(), SubmitRequest.QUICK).get(0);
        List<PipelineProblem> expected = List.of(
                new PipelineProblem(Severity.WARNING, Optional.empty(), Optional.empty(), Optional.of("lfc"),
                        "batch.note", "a note on the values file"),
                new PipelineProblem(Severity.WARNING, Optional.of("plot"), Optional.empty(), Optional.empty(),
                        "deps.font_missing", "plot_pareto: the font Liberation Sans is missing; gnuplot draws with its default"));
        assertEquals(expected, runs.view(f.arjun.id(), id).orElseThrow().warnings());
        f.awaitState(runs, id, RunState.FINISHED);
        RunView ended = runs.view(f.arjun.id(), id).orElseThrow();
        assertEquals(expected, ended.warnings());
        List<String> batch = Files.readAllLines(ended.record().runDir().orElseThrow().resolve("run.batch"));
        assertTrue(batch.get(2).startsWith("let $LFC "), batch.get(2));       // the lines the check named
        assertTrue(batch.get(21).startsWith("plot_pareto"), batch.get(21));
        // The run's record keeps them, so a restarted frontend still shows them.
        runs.close();
        DefaultRunService again = f.start(fake, client, 4, Duration.ofMillis(300), LockProbe.FILE_LOCKS);
        assertEquals(expected, again.view(f.arjun.id(), id).orElseThrow().warnings());
    }

    @Test
    void theResultsIndexStaysInsideTheRunDirectory() throws Exception {
        Path fake = f.fake("{\"after_ms\": 0, \"write\": {\"path\": \"plot_pareto/pareto.png\", \"text\": \"png\"}}",
                "{\"after_ms\": 0, \"exit\": 0}");
        DefaultRunService runs = f.start(fake, 4, Duration.ofMillis(300));
        RunId id = runs.submit(f.arjun.id(), f.toySession("results").id(), SubmitRequest.QUICK).get(0);
        f.awaitState(runs, id, RunState.FINISHED);
        FileResultsIndex index = new FileResultsIndex(runs::record);
        assertTrue(index.file(f.arjun.id(), id, "plot_pareto/pareto.png").isPresent());
        assertEquals(Optional.empty(), index.file(f.arjun.id(), id, "../../data/accounts.json"));
        assertEquals(Optional.empty(), index.file(f.arjun.id(), id, "/etc/passwd"));
        assertEquals("ok", index.manifest(f.arjun.id(), id).orElseThrow().path("status").asString());
        assertEquals(Optional.empty(), index.manifest(new fusion.frontend.accounts.AccountId(UUID.randomUUID()), id));
        // The session's last run gives the inputs' hashes for "changed since the last run".
        assertTrue(runs.lastRunInputHashes(runs.view(f.arjun.id(), id).orElseThrow().record().session()).isEmpty());
    }
}
