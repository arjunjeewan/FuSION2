package fusion.frontend.runner;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Clock;
import java.time.Duration;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.function.BooleanSupplier;

import fusion.frontend.accounts.Account;
import fusion.frontend.accounts.FileAccountService;
import fusion.frontend.pipeline.Catalog;
import fusion.frontend.pipeline.InputValue;
import fusion.frontend.pipeline.Json;
import fusion.frontend.pipeline.StandardBatchGenerator;
import fusion.frontend.pipeline.StandardPipelineValidator;
import fusion.frontend.pipeline.TestFixtures;
import fusion.frontend.sessions.FileSessionStore;
import fusion.frontend.sessions.Session;
import tools.jackson.databind.JsonNode;

/**
 * A runner over temporary folders: accounts, sessions, an in-process backend for the short calls,
 * and {@link FakeFusion} for the runs (section 12, Frontend contracts).
 */
final class RunnerFixture implements AutoCloseable {
    /** The short calls, answered in process, so a test starts one JVM per run only. */
    static final class StubBackend implements BackendClient {
        volatile String version = "0.1.0";
        volatile CheckReport check = new CheckReport(true, List.of());
        final Path home;
        final List<String> queries = new CopyOnWriteArrayList<>();

        StubBackend(Path home) {
            this.home = home;
        }

        @Override
        public BackendInfo info() {
            return new BackendInfo(version, Map.of(), home, Map.of(), List.of());
        }

        @Override
        public Catalog catalog() {
            return TestFixtures.catalog();
        }

        @Override
        public CheckReport check(Path batchFile) {
            return check;
        }

        @Override
        public JsonNode query(String batchLine, Duration timeout) {
            queries.add(batchLine);
            return Json.parse("{\"pairs\": [{\"id\": \"hsa1\", \"label\": \"SRC1\"}, {\"id\": \"hsa13\", \"label\": \"FSC13\"},"
                    + " {\"id\": \"hsa22\", \"label\": \"TGT22\"}, {\"id\": \"hsa2354\", \"label\": \"FOSB\"}]}", "labels");
        }
    }

    final Path root;
    final Path dataDir;
    final Path resultsDir;
    final FileAccountService accounts;
    final Account arjun;
    final FileSessionStore sessions;
    final StubBackend backend;
    final List<RunMessage> messages = new CopyOnWriteArrayList<>();
    final List<Integer> deliveries = new CopyOnWriteArrayList<>();
    private final List<DefaultRunService> services = new ArrayList<>();
    private DefaultRunService current;

    RunnerFixture(Path root) throws IOException {
        this.root = root;
        this.dataDir = root.resolve("data");
        this.resultsDir = root.resolve("results");
        this.accounts = new FileAccountService(dataDir, 4, Clock.systemUTC(), UUID::randomUUID);
        this.arjun = accounts.create("arjun", "Arjun", "password1".toCharArray());
        this.backend = new StubBackend(root.resolve("home"));
        this.sessions = new FileSessionStore(dataDir, TestFixtures::standard, accounts, () -> current, Clock.systemUTC(),
                UUID::randomUUID);
    }

    /** FuSION 1's toy case toy-hsa13-wni-tsv-b10, its inputs copied from the repository (pre-merged mode). */
    Session toySession(String name) throws IOException {
        Path inputs = Files.createDirectories(root.resolve("inputs"));
        Path toy = TestFixtures.repository().resolve("backend/tests/reference/synthetic/toy-hsa13-wni-tsv-b10/inputs");
        Path lfc = Files.copy(toy.resolve("with_node_included.tsv"), inputs.resolve("with_node_included.tsv"),
                java.nio.file.StandardCopyOption.REPLACE_EXISTING);
        Path graph = Files.copy(toy.resolve("report_example_with_pathways.xml"),
                inputs.resolve("report_example_with_pathways.xml"), java.nio.file.StandardCopyOption.REPLACE_EXISTING);
        Map<String, InputValue> values = new LinkedHashMap<>(TestFixtures.toyPremergedSession());
        values.put("lfc", new InputValue.PathValue(lfc));
        values.put("merged_graph", new InputValue.PathValue(graph));
        values.put("results_dir", new InputValue.PathValue(resultsDir));
        Session s = sessions.create(arjun.id(), name);
        return sessions.update(arjun.id(), s.id(), s.version(), x -> {
            Map<String, InputValue> all = new LinkedHashMap<>(x.inputs());
            all.putAll(values);
            return x.withInputs(all);
        });
    }

    /** The fake's launcher, with a scenario of JSON lines. */
    Path fake(String... scenario) throws IOException {
        Path dir = Files.createTempDirectory(root, "fake");
        Path script = dir.resolve("scenario.jsonl");
        Files.write(script, List.of(scenario));
        return FakeFusion.launcher(dir, Map.of("FAKE_FUSION_SCRIPT", script.toString()));
    }

    DefaultRunService start(Path executable, int cores, Duration grace) {
        return start(executable, cores, grace, LockProbe.FILE_LOCKS);
    }

    DefaultRunService start(Path executable, int cores, Duration grace, LockProbe locks) {
        return start(executable, backend, cores, grace, locks);
    }

    /** A runner whose short calls go to {@code client}, such as {@link FakeFusion} itself. */
    DefaultRunService start(Path executable, BackendClient client, int cores, Duration grace, LockProbe locks) {
        DefaultRunService s = new DefaultRunService(new DefaultRunService.Settings(dataDir, root.resolve("default-results"),
                executable, cores, Duration.ofMillis(40), grace, "0.1.0"), client, sessions, accounts,
                TestFixtures::standard, new StandardPipelineValidator(), new StandardBatchGenerator(),
                new GeneLookup(client, Duration.ofSeconds(5), dataDir), Clock.systemUTC(), UUID::randomUUID, locks);
        s.subscribe(arjun.id(), batch -> {
            deliveries.add(batch.size());
            messages.addAll(batch);
        });
        services.add(s);
        current = s;
        return s;
    }

    RunState state(DefaultRunService s, RunId id) {
        return s.view(arjun.id(), id).orElseThrow().record().state();
    }

    static void await(String what, BooleanSupplier condition) {
        long until = System.nanoTime() + Duration.ofSeconds(20).toNanos();
        while (!condition.getAsBoolean()) {
            if (System.nanoTime() > until) {
                throw new AssertionError("timed out waiting for " + what);
            }
            try {
                Thread.sleep(20);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                throw new AssertionError(e);
            }
        }
    }

    void awaitState(DefaultRunService s, RunId id, RunState state) {
        await(id + " to be " + state + " (now " + state(s, id) + ")", () -> state(s, id) == state);
    }

    @Override
    public void close() {
        services.forEach(DefaultRunService::close);
        // End any fake still running, children included.
        try (var dirs = Files.walk(resultsDir, 2)) {
            dirs.filter(p -> p.getFileName().toString().equals("run.lock")).forEach(lock -> {
                try {
                    long pid = Long.parseLong(Files.readString(lock).strip().split(" ")[0]);
                    if (pid != ProcessHandle.current().pid()) {
                        ProcessHandle.of(pid).ifPresent(DefaultRunService::killTree);
                    }
                } catch (IOException | RuntimeException e) {
                    // already gone
                }
            });
        } catch (IOException e) {
            // no results folder
        }
    }
}
