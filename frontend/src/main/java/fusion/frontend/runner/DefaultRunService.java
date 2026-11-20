package fusion.frontend.runner;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Clock;
import java.time.Duration;
import java.time.Instant;
import java.time.LocalDateTime;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.OptionalInt;
import java.util.OptionalLong;
import java.util.UUID;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.ReentrantLock;
import java.util.function.Consumer;
import java.util.function.Supplier;

import fusion.frontend.accounts.AccountId;
import fusion.frontend.accounts.AccountService;
import fusion.frontend.pipeline.AtomicFiles;
import fusion.frontend.pipeline.Binding;
import fusion.frontend.pipeline.Catalog;
import fusion.frontend.pipeline.CommandSpec;
import fusion.frontend.pipeline.DefaultPipeline;
import fusion.frontend.pipeline.GenerateOptions;
import fusion.frontend.pipeline.GeneratedBatch;
import fusion.frontend.pipeline.InputValue;
import fusion.frontend.pipeline.Json;
import fusion.frontend.pipeline.PipelineDocument;
import fusion.frontend.pipeline.PipelineException;
import fusion.frontend.pipeline.PipelineProblem;
import fusion.frontend.pipeline.PipelineValidator;
import fusion.frontend.pipeline.Provenance;
import fusion.frontend.pipeline.ResolvedPipeline;
import fusion.frontend.pipeline.Severity;
import fusion.frontend.pipeline.SideFile;
import fusion.frontend.pipeline.SourceMap;
import fusion.frontend.pipeline.SweepPoints;
import fusion.frontend.pipeline.Target;
import fusion.frontend.pipeline.ValueKind;
import fusion.frontend.pipeline.BatchGenerator;
import fusion.frontend.sessions.Session;
import fusion.frontend.sessions.SessionId;
import fusion.frontend.sessions.SessionStore;
import tools.jackson.databind.JsonNode;
import tools.jackson.databind.node.ObjectNode;

/**
 * The runner (A1, A2, 10.C5, 10.C16, 10.C19, 10.C20, 10.C21). Submit validates the session (S8.30),
 * generates its batch and queues one run per swept value; when the queue has the cores free, the run
 * gets a new directory under the session's results location (09.C1, S8.21), its {@code run.batch}
 * and {@code overrides/}, and one {@code fusion -b <dir>/run.batch --run-dir <dir> --tag
 * session=<id> --tag frontend_run=<id>} child, started in that directory with its console on the
 * null device (10.C17), so no pipe joins the run to the frontend. One scheduler thread polls every
 * active run's {@code events.jsonl} and run log every 250 ms and hands each subscriber one batch of
 * messages per tick. Stop writes the {@code stop} file (09.C7); Kill ends the process tree, once the
 * grace has passed (10.C19); Resume starts a new run that continues the old one (01.C6, 09.C5). After
 * a restart the runner reattaches to runs whose {@code run.lock} is held (10.C16).
 *
 * <p>FuSION 1 ran {@code ./fusion -b output_script.txt} in the GUI's own directory, read its output
 * through a pipe, guessed the stage from the echo, ignored the exit status and had no Stop
 * ({@code Linux/frontend/src/Gui4Panel.java:141-210}; S8.28).
 */
public final class DefaultRunService implements RunService, AutoCloseable {
    /** The runner's settings (section 10, Settings). */
    public record Settings(Path dataDir, Path defaultResultsDir, Path executable, int maxCores,
                           Duration pollInterval, Duration stopGrace, String frontendVersion) {}

    /** The session input that names a run's results location; the frontend's own input (10.C8). */
    public static final String RESULTS_INPUT = "results_dir";

    private static final System.Logger LOG = System.getLogger(DefaultRunService.class.getName());
    private static final boolean WINDOWS = System.getProperty("os.name").toLowerCase(java.util.Locale.ROOT)
            .startsWith("windows");

    private record Subscriber(AccountId who, Consumer<List<RunMessage>> perTick) {}

    /** A run with a process, or one that may have one: what the poller follows. */
    private static final class Live {
        final RunId id;
        final Path dir;
        final EventTail events;
        EventTail log;
        final RunProgress progress;
        final Process process;           // null when reattached: the frontend is not the parent
        volatile Integer exit;

        Live(RunId id, Path dir, RunProgress progress, Process process) {
            this.id = id;
            this.dir = dir;
            this.events = new EventTail(dir.resolve("events.jsonl"));
            this.log = new EventTail(dir.resolve(dir.getFileName() + ".log"));
            this.progress = progress;
            this.process = process;
        }
    }

    private final Settings settings;
    private final BackendClient backend;
    private final SessionStore sessions;
    private final AccountService accounts;
    private final DefaultPipeline pipelines;
    private final PipelineValidator validator;
    private final BatchGenerator generator;
    private final SubmitChecks checks;
    private final Clock clock;
    private final Supplier<UUID> ids;
    private final LockProbe locks;
    private final RunStore store;
    private final RunQueue queue;
    private final ReentrantLock lock = new ReentrantLock();
    private final Map<RunId, RunStore.Stored> runs = new LinkedHashMap<>();
    private final Map<RunId, Live> live = new LinkedHashMap<>();   // followed in the order they started
    private final Map<RunId, SourceMap> sourceMaps = new HashMap<>();
    private final Map<RunId, RunView> finishedViews = new HashMap<>();
    private final List<RunMessage> outbox = new ArrayList<>();
    private final List<Subscriber> subscribers = new CopyOnWriteArrayList<>();
    private final ScheduledExecutorService scheduler;
    private volatile Catalog lastCatalog;

    public DefaultRunService(Settings settings, BackendClient backend, SessionStore sessions, AccountService accounts,
                             DefaultPipeline pipelines, PipelineValidator validator, BatchGenerator generator,
                             GeneLookup genes, Clock clock, Supplier<UUID> ids) {
        this(settings, backend, sessions, accounts, pipelines, validator, generator, genes, clock, ids,
                LockProbe.FILE_LOCKS);
    }

    DefaultRunService(Settings settings, BackendClient backend, SessionStore sessions, AccountService accounts,
                      DefaultPipeline pipelines, PipelineValidator validator, BatchGenerator generator,
                      GeneLookup genes, Clock clock, Supplier<UUID> ids, LockProbe locks) {
        this.settings = settings;
        this.backend = backend;
        this.sessions = sessions;
        this.accounts = accounts;
        this.pipelines = pipelines;
        this.validator = validator;
        this.generator = generator;
        this.checks = new SubmitChecks(backend, genes, settings.dataDir());
        this.clock = clock;
        this.ids = ids;
        this.locks = locks;
        this.store = new RunStore(settings.dataDir());
        this.queue = new RunQueue(settings.maxCores());
        reattachAll();
        this.scheduler = Executors.newSingleThreadScheduledExecutor(r -> {
            Thread t = new Thread(r, "fusion-runner");
            t.setDaemon(true);
            return t;
        });
        long every = Math.max(10, settings.pollInterval().toMillis());
        scheduler.scheduleWithFixedDelay(this::tick, every, every, TimeUnit.MILLISECONDS);
    }

    /** Stops following; the runs themselves continue (10.C16). */
    @Override
    public void close() {
        scheduler.shutdownNow();
        try {
            scheduler.awaitTermination(5, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }

    // ---- Submit ---------------------------------------------------------------------------------

    @Override
    public List<RunId> submit(AccountId who, SessionId sessionId, SubmitRequest request) {
        Session session = sessions.get(who, sessionId).orElseThrow(() -> new RunnerException.NotOwner("session"));
        Prepared p = prepare(session, request.stopAfter(), OptionalInt.empty());
        UUID group = p.batches().size() > 1 ? ids.get() : null;
        List<RunId> made = new ArrayList<>();
        lock.lock();
        try {
            for (int i = 0; i < p.batches().size(); i++) {
                GeneratedBatch b = p.batches().get(i);
                RunId id = new RunId(ids.get());
                RunRecord r = new RunRecord(id, sessionId, who, p.choice(), b.sha256(), p.cores(), Optional.empty(),
                        Optional.ofNullable(group), group == null ? OptionalInt.empty() : OptionalInt.of(i),
                        Optional.empty(), Optional.empty(), clock.instant(), Optional.empty(), Optional.empty(),
                        OptionalLong.empty(), Optional.empty(), RunState.QUEUED, OptionalInt.empty());
                RunStore.Stored s = new RunStore.Stored(r, p.labels().get(i), Optional.of(p.resultsRoot()),
                        Optional.of(new RunStore.Pending(b.text(), b.sideFiles())), RunStore.ResumeMode.NONE,
                        Optional.empty(), Optional.empty(), false, Optional.empty(), p.warnings().get(i));
                put(s);
                sourceMaps.put(id, b.sourceMap());
                queue.add(id, p.cores());
                outbox.add(new RunMessage.Status(id, RunState.QUEUED, r.submitted()));
                made.add(id);
            }
        } finally {
            lock.unlock();
        }
        nudge();
        return made;
    }

    /** What validation gives: one batch per run, their labels, the cores each needs, each run's warnings. */
    private record Prepared(PipelineChoice choice, List<GeneratedBatch> batches, List<String> labels, int cores,
                            Path resultsRoot, List<List<PipelineProblem>> warnings) {}

    /**
     * Validation steps 1 to 5 (S8.30), then the batches: one per swept point (10.C21), or the one
     * point a restart reruns. Any error refuses Submit; warnings are shown and do not block (section
     * 10, Validation), so each run keeps those of steps 1 to 4 and its own batch's from step 5.
     */
    private Prepared prepare(Session session, Optional<String> stopAfter, OptionalInt onlyPoint) {
        PipelineDocument doc;
        try {
            doc = session.pipeline().orElseGet(pipelines::standard);
        } catch (PipelineException e) {
            throw new RunnerException.BackendUnavailable("the default pipeline is not available: " + e.getMessage(), e);
        }
        PipelineChoice choice = session.pipeline().isPresent() ? PipelineChoice.SESSION : PipelineChoice.DEFAULT;
        BackendInfo info = backend.info();
        if (!info.fusionVersion().equals(settings.frontendVersion())) {
            // One VERSION serves both, so a backend of another version is refused (11.C16).
            throw new RunnerException.BackendUnavailable("fusion " + info.fusionVersion()
                    + " is not this frontend's version " + settings.frontendVersion());
        }
        Catalog catalog = backend.catalog();
        lastCatalog = catalog;
        List<PipelineProblem> problems = new ArrayList<>(validator.validate(doc, session.inputs(), catalog));
        if (problems.stream().anyMatch(PipelineProblem::isError)) {
            throw new RunnerException.ValidationFailed(problems);
        }
        ResolvedPipeline resolved = ResolvedPipeline.of(doc, session.inputs(), session.overrides(), catalog);
        problems.addAll(checks.geneIds(doc, resolved, session.overrides()));
        if (problems.stream().anyMatch(PipelineProblem::isError)) {
            throw new RunnerException.ValidationFailed(problems);
        }
        Optional<Path> home = info.home().toString().isEmpty() ? Optional.empty() : Optional.of(info.home());
        SubmitChecks.PathResult paths = checks.paths(resolved, home);
        problems.addAll(paths.errors());
        if (problems.stream().anyMatch(PipelineProblem::isError)) {
            throw new RunnerException.ValidationFailed(problems);
        }
        List<PipelineProblem> common = List.copyOf(problems);     // only warnings are left at this point
        List<SweepPoints.Point> points = SweepPoints.points(doc, session.inputs());
        List<GeneratedBatch> batches = new ArrayList<>();
        List<String> labels = new ArrayList<>();
        List<List<PipelineProblem>> warnings = new ArrayList<>();
        Provenance prov = new Provenance(settings.frontendVersion(), session.id().toString());
        for (SweepPoints.Point point : points) {
            if (onlyPoint.isPresent() && point.index() != onlyPoint.getAsInt()) {
                continue;
            }
            GeneratedBatch b;
            try {
                OptionalInt index = point.label().isEmpty() ? OptionalInt.empty() : OptionalInt.of(point.index());
                b = generator.generate(doc, session.inputs(), session.overrides(), catalog,
                        new GenerateOptions(Target.RUN, stopAfter, index, prov));
            } catch (PipelineException e) {
                problems.add(new PipelineProblem(Severity.ERROR, Optional.empty(), Optional.empty(), Optional.empty(),
                        e.code(), e.getMessage()));
                throw new RunnerException.ValidationFailed(problems);
            }
            List<PipelineProblem> checked = checks.check(b, resolved, paths.missing());
            problems.addAll(checked);
            List<PipelineProblem> own = new ArrayList<>(common);
            checked.stream().filter(x -> !x.isError()).forEach(own::add);
            warnings.add(own);
            batches.add(b);
            labels.add(RunDirectories.label(session.name(), point.label()));
        }
        if (problems.stream().anyMatch(PipelineProblem::isError)) {
            throw new RunnerException.ValidationFailed(problems);
        }
        return new Prepared(choice, batches, labels, cores(resolved), resultsRoot(session), warnings);
    }

    /**
     * One core per FSC case of the run's solve (06.C4): its {@code cases}, or 2 with an FSC for
     * {@code in} and {@code out}, or 1 for {@code free}; 1 for a run without a solve (10.C20).
     */
    static int cores(ResolvedPipeline resolved) {
        int cores = 1;
        for (ResolvedPipeline.ResolvedBlock b : resolved.blocks()) {
            if (b.command().produces().filter(k -> k == ValueKind.RESULTS).isEmpty()) {
                continue;
            }
            Binding cases = b.args().get("cases");
            int n;
            if (cases != null) {
                n = resolved.valueOf(cases).map(v -> v instanceof InputValue.Items l ? l.items().size() : 1).orElse(1);
            } else {
                n = b.args().containsKey("fsc") ? 2 : 1;
            }
            cores = Math.max(cores, n);
        }
        return cores;
    }

    /** The session's results location, else the account's default, else {@code fusion.results-dir}. */
    private Path resultsRoot(Session session) {
        InputValue v = session.inputs().getOrDefault(RESULTS_INPUT, InputValue.UNSET);
        if (v instanceof InputValue.PathValue p) {
            return p.value();
        }
        return accounts.find(session.owner()).flatMap(a -> a.preferences().resultsDir())
                .orElse(settings.defaultResultsDir());
    }

    // ---- Stop, Kill, Resume ---------------------------------------------------------------------

    @Override
    public void stop(AccountId who, RunId run) {
        lock.lock();
        try {
            RunStore.Stored s = owned(who, run);
            RunState state = s.record().state();
            if (state == RunState.QUEUED) {
                queue.cancel(run);
                setState(s, s.record().withState(RunState.CANCELLED));
            } else if (state == RunState.RUNNING || state == RunState.STARTING) {
                Path dir = s.record().runDir().orElseThrow();
                Instant at = clock.instant();
                String user = accounts.find(who).map(a -> a.username()).orElse(who.toString());
                try {
                    // Only the file's existence matters; its text becomes the stop's note (09.C7, 1.31).
                    AtomicFiles.write(dir.resolve("stop"), "requested by " + user + " from the frontend at " + at + "\n",
                            false);
                } catch (IOException e) {
                    throw new RunnerException("run.stop_failed", "cannot write " + dir.resolve("stop") + ": "
                            + e.getMessage());
                }
                RunStore.Stored next = s.stopRequestedAt(at);
                setState(next, next.record().withState(RunState.STOPPING));
            }
        } finally {
            lock.unlock();
        }
        nudge();
    }

    @Override
    public void stopSweep(AccountId who, UUID sweepGroup) {
        List<RunId> members;
        lock.lock();
        try {
            members = runs.values().stream().map(RunStore.Stored::record)
                    .filter(r -> r.owner().equals(who) && r.sweepGroup().filter(sweepGroup::equals).isPresent())
                    .map(RunRecord::id).toList();
        } finally {
            lock.unlock();
        }
        members.forEach(id -> stop(who, id));
    }

    @Override
    public boolean killOffered(AccountId who, RunId run) {
        lock.lock();
        try {
            RunStore.Stored s = owned(who, run);
            return s.record().state() == RunState.STOPPING && s.stopRequested()
                    .map(t -> !clock.instant().isBefore(t.plus(settings.stopGrace()))).orElse(false);
        } finally {
            lock.unlock();
        }
    }

    @Override
    public void kill(AccountId who, RunId run) {
        lock.lock();
        try {
            RunStore.Stored s = owned(who, run);
            if (!killOffered(who, run)) {
                throw new RunnerException.NotKillable("Kill is offered once a stopped run has not ended within "
                        + settings.stopGrace().toSeconds() + " s of its stop request");
            }
            RunStore.Stored next = s.withKillRequested();
            put(next);
            processOf(next.record(), live.get(run)).ifPresent(DefaultRunService::killTree);
        } finally {
            lock.unlock();
        }
        nudge();
    }

    /**
     * Ends the process and every descendant (10.C19): {@code ProcessHandle.descendants()} first, then
     * {@code destroyForcibly}; on Windows, ending a parent does not end its children.
     */
    static void killTree(ProcessHandle root) {
        List<ProcessHandle> descendants = root.descendants().toList();
        root.destroyForcibly();
        descendants.forEach(ProcessHandle::destroyForcibly);
    }

    /** The run's process: the child, or by its recorded id and start time after a restart. */
    private static Optional<ProcessHandle> processOf(RunRecord r, Live l) {
        if (l != null && l.process != null) {
            return Optional.of(l.process.toHandle());
        }
        if (r.pid().isEmpty()) {
            return Optional.empty();
        }
        return ProcessHandle.of(r.pid().getAsLong()).filter(ProcessHandle::isAlive)
                .filter(h -> r.processStart().isEmpty() || h.info().startInstant().map(t -> t.equals(r.processStart().get()))
                        .orElse(true));
    }

    @Override
    public RunId resume(AccountId who, RunId run) {
        RunStore.Stored old;
        lock.lock();
        try {
            old = owned(who, run);
        } finally {
            lock.unlock();
        }
        RunRecord r = old.record();
        if (!r.state().isResumable() || r.runDir().isEmpty()) {
            throw new RunnerException.NotResumable(r.state());
        }
        Optional<RunStore.Pending> pending = Optional.empty();
        RunStore.ResumeMode mode = RunStore.ResumeMode.CONTINUE;
        String sha = r.batchSha256();
        SourceMap map = null;
        int cores = r.cores();
        List<PipelineProblem> warnings = List.of();
        if (r.state() == RunState.FAILED) {
            // S1.28: the session's current batch, which may hold the fix, runs with the failed run as parent.
            Session session = sessions.get(who, r.session()).orElseThrow(() -> new RunnerException.NotOwner("session"));
            Prepared p = prepare(session, Optional.empty(), r.sweepValue());
            GeneratedBatch b = p.batches().get(0);
            pending = Optional.of(new RunStore.Pending(b.text(), b.sideFiles()));
            mode = RunStore.ResumeMode.RESTART;
            sha = b.sha256();
            map = b.sourceMap();
            cores = p.cores();
            warnings = p.warnings().get(0);
        }
        RunId id = new RunId(ids.get());
        lock.lock();
        try {
            RunRecord next = new RunRecord(id, r.session(), who, r.pipeline(), sha, cores, Optional.empty(),
                    r.sweepGroup(), r.sweepValue(), Optional.of(run), Optional.empty(), clock.instant(),
                    Optional.empty(), Optional.empty(), OptionalLong.empty(), Optional.empty(), RunState.QUEUED,
                    OptionalInt.empty());
            put(new RunStore.Stored(next, old.label(), old.resultsRoot(), pending, mode, r.runDir(), Optional.empty(),
                    false, Optional.empty(), warnings));
            if (map != null) {
                sourceMaps.put(id, map);
            }
            queue.add(id, cores);
            outbox.add(new RunMessage.Status(id, RunState.QUEUED, next.submitted()));
        } finally {
            lock.unlock();
        }
        nudge();
        return id;
    }

    // ---- views ----------------------------------------------------------------------------------

    @Override
    public Optional<RunView> view(AccountId who, RunId run) {
        lock.lock();
        try {
            RunStore.Stored s = runs.get(run);
            if (s == null || !s.record().owner().equals(who)) {
                return Optional.empty();
            }
            Live l = live.get(run);
            if (l != null) {
                return Optional.of(viewOf(s.record(), l.progress, s.warnings()));
            }
            RunView cached = finishedViews.get(run);
            if (cached != null && cached.record().equals(s.record())) {
                return Optional.of(cached);
            }
            RunView v = replay(s);
            if (!s.record().state().isActive()) {
                finishedViews.put(run, v);
            }
            return Optional.of(v);
        } finally {
            lock.unlock();
        }
    }

    private RunView viewOf(RunRecord r, RunProgress p, List<PipelineProblem> warnings) {
        return new RunView(r, p.progress(clock.instant()), p.reports(), p.caseStatuses(), p.failure(), warnings);
    }

    /** A run not followed now is read from its events file from the start: 30 to 40 KB (10.C5). */
    private RunView replay(RunStore.Stored s) {
        RunProgress p = progressFor(s.record().id());
        s.note().ifPresent(n -> {
            ObjectNode f = Json.object();
            f.put("code", s.record().state() == RunState.NOT_STARTED ? "process.not_started" : "run.note");
            f.put("message", n);
            p.setFailure(f);
        });
        s.record().runDir().ifPresent(dir -> {
            try {
                for (EventTail.Line line : new EventTail(dir.resolve("events.jsonl")).poll()) {
                    p.apply(line.text());
                }
            } catch (IOException e) {
                LOG.log(System.Logger.Level.WARNING, "cannot read the events of " + dir + ": " + e.getMessage());
            }
        });
        return viewOf(s.record(), p, s.warnings());
    }

    private RunProgress progressFor(RunId id) {
        SourceMap map = sourceMaps.get(id);
        return new RunProgress(cmd -> Optional.ofNullable(lastCatalog).flatMap(c -> c.command(cmd))
                .map(CommandSpec::stage).filter(st -> !st.isEmpty()),
                line -> map == null ? Optional.empty() : map.block(line));
    }

    @Override
    public List<RunRecord> runsOf(AccountId who, SessionId session) {
        lock.lock();
        try {
            return runs.values().stream().map(RunStore.Stored::record)
                    .filter(r -> r.owner().equals(who) && r.session().equals(session))
                    .sorted(Comparator.comparing(RunRecord::submitted).thenComparing(r -> r.id().toString()))
                    .toList();
        } finally {
            lock.unlock();
        }
    }

    /** One run's record, for the results index. */
    Optional<RunRecord> record(AccountId who, RunId run) {
        lock.lock();
        try {
            RunStore.Stored s = runs.get(run);
            return s == null || !s.record().owner().equals(who) ? Optional.empty() : Optional.of(s.record());
        } finally {
            lock.unlock();
        }
    }

    @Override
    public AutoCloseable subscribe(AccountId who, Consumer<List<RunMessage>> perTick) {
        Subscriber s = new Subscriber(who, perTick);
        subscribers.add(s);
        return () -> subscribers.remove(s);
    }

    // ---- SessionUse -----------------------------------------------------------------------------

    @Override
    public boolean busy(SessionId session) {
        lock.lock();
        try {
            return runs.values().stream().anyMatch(s -> s.record().session().equals(session)
                    && s.record().state().isActive());
        } finally {
            lock.unlock();
        }
    }

    @Override
    public Map<Path, String> lastRunInputHashes(SessionId session) {
        Optional<Path> dir;
        lock.lock();
        try {
            dir = runs.values().stream().map(RunStore.Stored::record)
                    .filter(r -> r.session().equals(session) && r.runDir().isPresent())
                    .max(Comparator.comparing(RunRecord::submitted)).flatMap(RunRecord::runDir);
        } finally {
            lock.unlock();
        }
        Map<Path, String> out = new HashMap<>();
        dir.flatMap(DefaultRunService::readRunJson).ifPresent(json -> {
            for (JsonNode in : json.path("inputs")) {
                Optional<String> path = Json.text(in, "path");
                Optional<String> sha = Json.text(in, "sha256");
                if (path.isPresent() && sha.isPresent()) {
                    out.put(Path.of(path.get()), sha.get());
                }
            }
        });
        return out;
    }

    // ---- the poll tick --------------------------------------------------------------------------

    private void nudge() {
        try {
            scheduler.execute(this::tick);
        } catch (java.util.concurrent.RejectedExecutionException e) {
            // closed
        }
    }

    void tick() {
        List<RunMessage> messages;
        lock.lock();
        try {
            startQueued();
            for (Live l : new ArrayList<>(live.values())) {
                follow(l);
            }
            messages = List.copyOf(outbox);
            outbox.clear();
        } catch (RuntimeException e) {
            LOG.log(System.Logger.Level.ERROR, "runner tick failed", e);
            return;
        } finally {
            lock.unlock();
        }
        if (messages.isEmpty()) {
            return;
        }
        for (Subscriber s : subscribers) {
            List<RunMessage> mine = messages.stream().filter(m -> owner(m.run()).filter(s.who()::equals).isPresent())
                    .toList();
            if (!mine.isEmpty()) {
                try {
                    s.perTick().accept(mine);
                } catch (RuntimeException e) {
                    LOG.log(System.Logger.Level.WARNING, "a run subscriber failed", e);
                }
            }
        }
    }

    private Optional<AccountId> owner(RunId id) {
        lock.lock();
        try {
            return Optional.ofNullable(runs.get(id)).map(s -> s.record().owner());
        } finally {
            lock.unlock();
        }
    }

    private void startQueued() {
        int inUse = live.keySet().stream().mapToInt(id -> runs.get(id).record().cores()).sum();
        for (RunId id : queue.startable(inUse)) {
            start(runs.get(id));
        }
    }

    private void start(RunStore.Stored s) {
        RunRecord r = s.record();
        Path dir;
        try {
            Path root = s.resultsRoot().orElse(settings.defaultResultsDir());
            dir = RunDirectories.create(root, s.label(), LocalDateTime.now(clock));
            // Recorded as starting, with its directory, before the process exists: a frontend that
            // dies here finds the directory on restart, and never starts the run twice (10.C16).
            s = s.with(r.withRunDir(dir).withState(RunState.STARTING));
            put(s);
            outbox.add(new RunMessage.Status(r.id(), RunState.STARTING, clock.instant()));
            if (s.pending().isPresent()) {
                // run.batch and overrides/ at the top of the run directory (1.31, 1.32).
                Files.writeString(dir.resolve("run.batch"), s.pending().get().batch());
                for (SideFile f : s.pending().get().sideFiles()) {
                    Path side = dir.resolve(f.relativePath());
                    Files.createDirectories(side.getParent());
                    Files.writeString(side, f.text());
                }
            }
        } catch (IOException e) {
            notStarted(s, "cannot make the run directory: " + e.getMessage());
            return;
        }
        List<String> command = new ArrayList<>();
        command.add(settings.executable().toString());
        if (s.resume() != RunStore.ResumeMode.CONTINUE) {
            command.add("-b");
            command.add(dir.resolve("run.batch").toString());
        }
        if (s.resume() != RunStore.ResumeMode.NONE) {
            command.add("--resume");
            command.add(s.resumeDir().orElseThrow().toString());
        }
        command.add("--run-dir");
        command.add(dir.toString());
        command.add("--tag");
        command.add("session=" + r.session());
        command.add("--tag");
        command.add("frontend_run=" + r.id());
        ProcessBuilder pb = new ProcessBuilder(command).directory(dir.toFile())
                .redirectInput(ProcessBuilder.Redirect.from(ProcessBackendClient.nullDevice()))
                .redirectOutput(ProcessBuilder.Redirect.DISCARD)
                .redirectError(ProcessBuilder.Redirect.DISCARD);
        Process process;
        try {
            process = pb.start();
        } catch (IOException e) {
            notStarted(s, "cannot start " + settings.executable() + ": " + e.getMessage());
            return;
        }
        Instant now = clock.instant();
        RunRecord running = s.record().started(dir, process.pid(), process.info().startInstant(), now);
        RunStore.Stored started = s.started(running);
        put(started);
        outbox.add(new RunMessage.Status(r.id(), RunState.RUNNING, now));
        Live l = new Live(r.id(), dir, progressFor(r.id()), process);
        live.put(r.id(), l);
        process.onExit().thenAccept(p -> {
            l.exit = p.exitValue();
            nudge();
        });
    }

    private void notStarted(RunStore.Stored s, String why) {
        LOG.log(System.Logger.Level.WARNING, "run " + s.record().id() + " not started: " + why);
        RunStore.Stored next = s.withNote(why);
        setState(next, next.record().ended(RunState.NOT_STARTED, clock.instant(), OptionalInt.empty()));
    }

    /** One poll of a followed run: new events and log text, then its end, if it has ended. */
    private void follow(Live l) {
        drain(l);
        RunStore.Stored s = runs.get(l.id);
        if (l.process != null) {
            Integer exit = l.exit;
            if (exit != null) {
                drain(l);
                Optional<JsonNode> json = readRunJson(l.dir);
                Optional<String> status = json.flatMap(j -> Json.text(j, "status")).filter(ExitMapping::isFinal);
                RunState state = ExitMapping.state(exit, s.killRequested(), status);
                end(l, s, state, OptionalInt.of(exit), json);
            }
            return;
        }
        // Reattached: the frontend is not the parent, so the end comes from run.json and run.lock (10.C16).
        Optional<JsonNode> json = readRunJson(l.dir);
        Optional<String> status = json.flatMap(j -> Json.text(j, "status")).filter(ExitMapping::isFinal);
        if (status.isPresent()) {
            drain(l);
            OptionalInt exit = json.get().path("exit").isIntegralNumber()
                    ? OptionalInt.of(json.get().get("exit").asInt()) : OptionalInt.empty();
            RunState state = s.killRequested() ? RunState.KILLED : ExitMapping.fromRunJson(status.get());
            end(l, s, state, exit, json);
        } else if (!alive(s.record())) {
            drain(l);
            end(l, s, s.killRequested() ? RunState.KILLED : RunState.LOST, OptionalInt.empty(), json);
        }
    }

    private void drain(Live l) {
        try {
            for (EventTail.Line line : l.events.poll()) {
                Optional<JsonNode> e = l.progress.apply(line.text());
                if (e.isPresent()) {
                    outbox.add(new RunMessage.Event(l.id, e.get().path("seq").asLong(0), e.get()));
                    if (e.get().path("type").asString("").equals("run.started")) {
                        // fusion's own run id; written with the record's next change, run.json has it too.
                        Optional<String> fusionId = Json.text(e.get(), "run");
                        RunStore.Stored s = runs.get(l.id);
                        if (fusionId.isPresent() && s.record().fusionRunId().isEmpty()) {
                            runs.put(l.id, s.with(s.record().withFusionRunId(fusionId.get())));
                        }
                    }
                }
            }
            if (l.log.offset() == 0 && !Files.exists(l.log.file())) {
                // The log's name is the run.log setting, the run's name by default (09.C11).
                readRunJson(l.dir).flatMap(DefaultRunService::logName).ifPresent(name ->
                        l.log = new EventTail(l.dir.resolve(name)));
            }
            List<EventTail.Line> lines = l.log.poll();
            if (!lines.isEmpty()) {
                StringBuilder text = new StringBuilder();
                lines.forEach(line -> text.append(line.text()).append('\n'));
                outbox.add(new RunMessage.Log(l.id, lines.get(0).offset(), text.toString()));
            }
        } catch (IOException e) {
            LOG.log(System.Logger.Level.WARNING, "cannot follow " + l.dir + ": " + e.getMessage());
        }
    }

    private static Optional<String> logName(JsonNode runJson) {
        for (JsonNode s : runJson.path("run_settings")) {
            if (s.path("key").asString("").equals("run.log") && s.path("value").isString()) {
                return Optional.of(s.get("value").stringValue());
            }
        }
        return Optional.empty();
    }

    private void end(Live l, RunStore.Stored s, RunState state, OptionalInt exit, Optional<JsonNode> json) {
        Instant at = clock.instant();
        RunStore.Stored next = s;
        if (state == RunState.CRASHED && exit.isPresent()) {
            next = s.withNote(ExitMapping.describe(exit.getAsInt(), WINDOWS));
            ObjectNode f = Json.object();
            f.put("code", "run.crashed");
            f.put("message", next.note().orElse(""));
            l.progress.setFailure(f);
        }
        if (json.isPresent() && json.get().path("stop").path("forced").asBoolean(false)) {
            next = next.withNote("stopped by force, 30 s after the stop request (09.C7)");
        }
        json.flatMap(j -> Optional.ofNullable(j.get("error"))).filter(JsonNode::isObject)
                .ifPresent(l.progress::setFailure);
        RunRecord ended = next.record().ended(state, at, exit);
        setState(next, ended);
        live.remove(l.id);
        finishedViews.put(l.id, viewOf(ended, l.progress, next.warnings()));
    }

    private boolean alive(RunRecord r) {
        Path dir = r.runDir().orElseThrow();
        return switch (locks.probe(dir.resolve("run.lock"))) {
            case HELD -> true;
            case FREE -> false;
            // A filesystem without locks: the process id and its start time (10.C16).
            case UNSUPPORTED -> processOf(r, null).isPresent();
        };
    }

    // ---- records --------------------------------------------------------------------------------

    private RunStore.Stored owned(AccountId who, RunId run) {
        RunStore.Stored s = runs.get(run);
        if (s == null || !s.record().owner().equals(who)) {
            throw new RunnerException.NotOwner("run");
        }
        return s;
    }

    private void put(RunStore.Stored s) {
        store.save(s);
        runs.put(s.record().id(), s);
    }

    private void setState(RunStore.Stored s, RunRecord next) {
        put(s.with(next));
        outbox.add(new RunMessage.Status(next.id(), next.state(), clock.instant()));
    }

    static Optional<JsonNode> readRunJson(Path dir) {
        Path f = dir.resolve("run.json");
        if (!Files.isRegularFile(f)) {
            return Optional.empty();
        }
        try {
            return Optional.of(Json.parse(Files.readString(f), f.toString()));
        } catch (IOException | RuntimeException e) {
            return Optional.empty();
        }
    }

    /** At start: final records are indexed, queued runs requeued, live ones reattached (10.C16). */
    private void reattachAll() {
        List<RunStore.Stored> all = new ArrayList<>(store.loadAll());
        all.sort(Comparator.comparing((RunStore.Stored s) -> s.record().submitted())
                .thenComparing(s -> s.record().id().toString()));
        for (RunStore.Stored s : all) {
            runs.put(s.record().id(), s);
            RunRecord r = s.record();
            if (r.state() == RunState.QUEUED) {
                queue.add(r.id(), r.cores());
            } else if (r.state().isLive()) {
                reattach(s);
            }
        }
    }

    private void reattach(RunStore.Stored s) {
        RunRecord r = s.record();
        if (r.runDir().isEmpty()) {
            setState(s, r.ended(RunState.LOST, clock.instant(), OptionalInt.empty()));
            return;
        }
        Path dir = r.runDir().get();
        Optional<JsonNode> json = readRunJson(dir);
        Optional<String> status = json.flatMap(j -> Json.text(j, "status")).filter(ExitMapping::isFinal);
        if (status.isPresent()) {
            OptionalInt exit = json.get().path("exit").isIntegralNumber()
                    ? OptionalInt.of(json.get().get("exit").asInt()) : OptionalInt.empty();
            Instant at = Json.text(json.get(), "finished").map(Instant::parse).orElse(clock.instant());
            setState(s, r.ended(s.killRequested() ? RunState.KILLED : ExitMapping.fromRunJson(status.get()), at, exit));
        } else if (alive(r)) {
            // Follow its files from offset 0, since the files are the log.
            live.put(r.id(), new Live(r.id(), dir, progressFor(r.id()), null));
        } else {
            setState(s, r.ended(s.killRequested() ? RunState.KILLED : RunState.LOST, clock.instant(), OptionalInt.empty()));
        }
    }

    // ---- for the screens ------------------------------------------------------------------------

    /** How many of the account's runs are queued, starting, running or stopping (10.C16's count). */
    public long activeRuns(AccountId who) {
        lock.lock();
        try {
            return runs.values().stream().map(RunStore.Stored::record)
                    .filter(r -> r.owner().equals(who) && r.state().isActive()).count();
        } finally {
            lock.unlock();
        }
    }

    /** The last catalog fusion gave, for stage labels and the block library. */
    public Optional<Catalog> catalog() {
        return Optional.ofNullable(lastCatalog);
    }

    /** Asks fusion for its version and catalog at start (section 10, Backend checks at start). */
    public void checkBackend() {
        BackendInfo info = backend.info();
        if (!info.fusionVersion().equals(settings.frontendVersion())) {
            throw new RunnerException.BackendUnavailable("fusion " + info.fusionVersion()
                    + " is not this frontend's version " + settings.frontendVersion());
        }
        lastCatalog = backend.catalog();
    }
}
