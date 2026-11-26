package fusion.frontend.ui;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.awt.image.BufferedImage;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Clock;
import java.time.Duration;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.OptionalInt;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;
import java.util.UUID;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.TimeUnit;
import java.util.function.Supplier;
import java.util.stream.Collectors;

import javax.imageio.ImageIO;

import org.junit.jupiter.api.Assumptions;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.condition.EnabledIfEnvironmentVariable;
import org.junit.jupiter.api.io.TempDir;

import javafx.application.Platform;
import javafx.geometry.Bounds;
import javafx.scene.Node;
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.scene.canvas.Canvas;
import javafx.scene.control.Button;
import javafx.scene.control.ListView;
import javafx.scene.control.ScrollPane;
import javafx.scene.control.TabPane;
import javafx.scene.control.TableView;
import javafx.scene.control.TextArea;
import javafx.scene.image.ImageView;
import javafx.scene.image.PixelReader;
import javafx.scene.image.WritableImage;
import javafx.stage.Stage;

import fusion.frontend.accounts.Account;
import fusion.frontend.accounts.FileAccountService;
import fusion.frontend.pipeline.Catalog;
import fusion.frontend.pipeline.FrontendVersion;
import fusion.frontend.pipeline.InputValue;
import fusion.frontend.pipeline.Json;
import fusion.frontend.pipeline.Severity;
import fusion.frontend.pipeline.StandardBatchGenerator;
import fusion.frontend.pipeline.StandardPipelineValidator;
import fusion.frontend.pipeline.TestFixtures;
import fusion.frontend.runner.BackendClient;
import fusion.frontend.runner.BackendInfo;
import fusion.frontend.runner.CheckDiagnostic;
import fusion.frontend.runner.CheckReport;
import fusion.frontend.runner.DefaultRunService;
import fusion.frontend.runner.ExpressionQueries;
import fusion.frontend.runner.FileResultsIndex;
import fusion.frontend.runner.GeneLookup;
import fusion.frontend.runner.ProcessBackendClient;
import fusion.frontend.runner.RunRecord;
import fusion.frontend.runner.RunState;
import fusion.frontend.runner.RunView;
import fusion.frontend.runner.StandardPipelineLoader;
import fusion.frontend.sessions.FileSessionChecks;
import fusion.frontend.sessions.FileSessionStore;
import fusion.frontend.sessions.Session;
import tools.jackson.databind.JsonNode;

/**
 * The screens build and draw (Phase 5 designs and tests them; this checks the minimal ones of
 * decision 0038). Off-screen: no window is shown; each scene is drawn into
 * {@code build/screens/*.png}. It needs a display, which CI's Xvfb gives (11, ci.yml). One test
 * draws a real run, for the release notes (poll 333), when the backend's staged build exists.
 */
@EnabledIfEnvironmentVariable(named = "DISPLAY", matches = ".+")
class ScreensSnapshotTest {
    @TempDir
    Path tmp;

    private static final String RESULTS = "{\\\"format\\\": \\\"fusion.results\\\", \\\"version\\\": 1, \\\"cases\\\": ["
            + "{\\\"case\\\": \\\"in\\\", \\\"points\\\": [{\\\"n\\\": 1, \\\"e\\\": 0, \\\"certified\\\": true},"
            + " {\\\"n\\\": 0, \\\"e\\\": 2, \\\"certified\\\": true}]},"
            + " {\\\"case\\\": \\\"out\\\", \\\"points\\\": [{\\\"n\\\": 2, \\\"e\\\": 1, \\\"certified\\\": false}]}]}";

    /** fusion's classification of the values as cudf gives it, and its suggestions (04.C8). */
    private static final String PREVIEW = "{\"ok\": true, \"read\": {\"skipped\": []}, \"n_up\": 2, \"n_down\": 1,"
            + " \"n_none\": 0, \"genes\": [{\"id\": \"hsa1956\", \"value\": 1.5, \"class\": \"up\"},"
            + " {\"id\": \"hsa13\", \"value\": 1.2, \"class\": \"up\"}, {\"id\": \"hsa22\", \"value\": -1.3, \"class\": \"down\"}]}";
    private static final String SUGGESTIONS = "{\"ok\": true, \"rows\": ["
            + "{\"criterion\": \"current\", \"up\": 1.01, \"down\": -1.01, \"n_up\": 2, \"n_down\": 1, \"conflicts\": 0},"
            + " {\"criterion\": \"quantile 5%\", \"up\": 1.2, \"down\": -1.3, \"n_up\": 2, \"n_down\": 1, \"conflicts\": 0}],"
            + " \"histogram\": {\"lo\": -2.0, \"width\": 0.25, \"counts\": [0, 1, 2, 0, 0, 0, 0, 0, 0, 0, 1, 3, 2, 1, 0, 0]}}";

    /** Answers for the short calls, in process. */
    static final class Backend implements BackendClient {
        @Override
        public BackendInfo info() {
            return new BackendInfo("0.1.0", Map.of(), Path.of("/opt/fusion/share/fusion"), Map.of(), List.of());
        }

        @Override
        public Catalog catalog() {
            return TestFixtures.catalog();
        }

        @Override
        public CheckReport check(Path batchFile) {
            // A warning on the toy batch's plot_pareto line: shown with the run, and it does not block.
            return new CheckReport(true, List.of(new CheckDiagnostic(Severity.WARNING, 22, OptionalInt.empty(),
                    "deps.font_missing", "plot_pareto: the font Liberation Sans is missing; gnuplot draws with its default",
                    Optional.empty(), Optional.empty())));
        }

        @Override
        public JsonNode query(String batchLine, Duration timeout) {
            return Json.parse(switch (batchLine.split(" ", 2)[0]) {
                case "cudf" -> PREVIEW;
                case "suggest_thresholds" -> SUGGESTIONS;
                default -> "{\"pairs\": [{\"id\": \"hsa1956\", \"label\": \"EGFR\"}, [\"hsa1\", \"SRC1\"],"
                        + " [\"hsa13\", \"FSC13\"], [\"hsa22\", \"TGT22\"]]}";
            }, batchLine);
        }
    }

    private static synchronized void startFx() throws Exception {
        CompletableFuture<Void> up = new CompletableFuture<>();
        try {
            Platform.startup(() -> up.complete(null));
        } catch (IllegalStateException e) {
            up.complete(null);           // already started by another test
        }
        up.get(20, TimeUnit.SECONDS);
        Platform.setImplicitExit(false);
    }

    private static <T> T onFx(Supplier<T> work) throws Exception {
        CompletableFuture<T> f = new CompletableFuture<>();
        Platform.runLater(() -> {
            try {
                f.complete(work.get());
            } catch (RuntimeException e) {
                f.completeExceptionally(e);
            }
        });
        return f.get(20, TimeUnit.SECONDS);
    }

    /**
     * Waits until the screen shows what a test needs, checked on the FX thread every 25 ms. An
     * off-screen scene gets no pulses, so CSS and layout are applied first: they make the skins that
     * hold the content of tabs, titled panes and scroll panes.
     */
    private static void until(Parent root, String what, Supplier<Boolean> shown) throws Exception {
        long end = System.nanoTime() + 20_000_000_000L;
        while (!onFx(() -> {
            root.applyCss();
            root.layout();
            return shown.get();
        })) {
            if (System.nanoTime() > end) {
                throw new AssertionError("the screen never showed " + what);
            }
            Thread.sleep(25);
        }
    }

    private static boolean hasLabel(Parent root, String text) {
        return root.lookupAll(".label").stream()
                .anyMatch(n -> n instanceof javafx.scene.control.Label l && text.equals(l.getText()));
    }

    private static boolean labelHolds(Parent root, String text) {
        return root.lookupAll(".label").stream()
                .anyMatch(n -> n instanceof javafx.scene.control.Label l && l.getText() != null && l.getText().contains(text));
    }

    private static javafx.scene.control.ListView<?> list(Parent root) {
        return (javafx.scene.control.ListView<?>) root.lookup(".list-view");
    }

    private static boolean hasField(Parent root, String text) {
        return root.lookupAll(".text-field").stream()
                .anyMatch(n -> n instanceof javafx.scene.control.TextField t && text.equals(t.getText()));
    }

    private static boolean listHolds(Parent root, String item) {
        return root.lookupAll(".list-view").stream()
                .anyMatch(n -> n instanceof javafx.scene.control.ListView<?> l && l.getItems().contains(item));
    }

    private static void fire(Parent root, String button) {
        root.lookupAll(".button").stream().map(n -> (Button) n).filter(b -> button.equals(b.getText())).findFirst()
                .orElseThrow(() -> new AssertionError("no button " + button)).fire();
    }

    /** The table whose first column is {@code column}, such as the suggestions' Criterion. */
    private static Optional<TableView<?>> table(Parent root, String column) {
        return root.lookupAll(".table-view").stream().<TableView<?>>map(n -> (TableView<?>) n)
                .filter(t -> !t.getColumns().isEmpty() && column.equals(t.getColumns().get(0).getText())).findFirst();
    }

    /** A table's row as its cells show it. */
    private static List<String> row(TableView<?> t, int index) {
        List<String> cells = new ArrayList<>();
        t.getColumns().forEach(c -> cells.add(String.valueOf(c.getCellData(index))));
        return cells;
    }

    /** The run's details: the tabs Log, Frontier, Pareto plot and Explanation pages. */
    private static TabPane details(Parent root) {
        return root.lookupAll(".tab-pane").stream().map(n -> (TabPane) n)
                .filter(t -> t.getTabs().get(0).getText().equals("Log")).findFirst().orElseThrow();
    }

    private static boolean logHolds(Parent root, List<String> lines) {
        return root.lookupAll(".text-area").stream().map(n -> ((TextArea) n).getText())
                .anyMatch(t -> lines.stream().allMatch(t::contains));
    }

    /** FuSION 1's frontier of one case, from its {@code .dat} file: a node and an edge count per line. */
    private static Set<List<Integer>> dat(Path file) throws IOException {
        Set<List<Integer>> points = new TreeSet<>(Comparator.<List<Integer>>comparingInt(p -> p.get(0))
                .thenComparingInt(p -> p.get(1)));
        for (String line : Files.readAllLines(file)) {
            if (!line.isBlank()) {
                String[] cols = line.strip().split("\\s+");
                points.add(List.of(Integer.parseInt(cols[0]), Integer.parseInt(cols[1])));
            }
        }
        return points;
    }

    private static void snapshot(Parent root, Path png, int w, int h) throws Exception {
        WritableImage image = onFx(() -> (root.getScene() != null ? root.getScene() : new Scene(root, w, h)).snapshot(null));
        PixelReader px = image.getPixelReader();
        BufferedImage out = new BufferedImage((int) image.getWidth(), (int) image.getHeight(), BufferedImage.TYPE_INT_ARGB);
        for (int y = 0; y < out.getHeight(); y++) {
            for (int x = 0; x < out.getWidth(); x++) {
                out.setRGB(x, y, px.getArgb(x, y));
            }
        }
        Files.createDirectories(png.getParent());
        ImageIO.write(out, "png", png.toFile());
    }

    @Test
    void theSignInAndMainScreensDraw() throws Exception {
        startFx();
        Path data = tmp.resolve("data");
        FileAccountService accounts = new FileAccountService(data, 4, Clock.systemUTC(), UUID::randomUUID);
        Backend backend = new Backend();
        DefaultRunService[] runs = new DefaultRunService[1];
        FileSessionStore sessions = new FileSessionStore(data, TestFixtures::standard, accounts, () -> runs[0],
                Clock.systemUTC(), UUID::randomUUID);
        // A run that leaves section 07's results.json behind, through the runner's test fake.
        Path scenario = Files.createDirectories(tmp.resolve("fake")).resolve("scenario.jsonl");
        Files.write(scenario, List.of(
                "{\"after_ms\": 0, \"event\": {\"type\": \"step.started\", \"step\": \"pathz3\", \"cmd\": \"pathz3\"}}",
                "{\"after_ms\": 0, \"event\": {\"type\": \"solve.case.finished\", \"case\": \"in\", \"status\": \"complete\"}}",
                "{\"after_ms\": 0, \"write\": {\"path\": \"pathz3/results.json\", \"text\": \"" + RESULTS + "\"}}",
                "{\"after_ms\": 0, \"log\": \"info  with FSC (FSC13): 2 Pareto points, all certified\"}",
                "{\"after_ms\": 0, \"exit\": 0}"));
        Path fake = fusion.frontend.runner.FakeFusion.launcher(scenario.getParent(), Map.of("FAKE_FUSION_SCRIPT",
                scenario.toString()));
        GeneLookup genes = new GeneLookup(backend, Duration.ofSeconds(5), data);
        runs[0] = new DefaultRunService(new DefaultRunService.Settings(data, tmp.resolve("runs"), fake,
                4, Duration.ofMillis(100), Duration.ofSeconds(60), "0.1.0"), backend, sessions, accounts,
                TestFixtures::standard, new StandardPipelineValidator(), new StandardBatchGenerator(),
                genes, Clock.systemUTC(), UUID::randomUUID);
        Path inputs = TestFixtures.repository().resolve("backend/tests/reference/synthetic/toy-hsa13-wni-tsv-b10/inputs");
        try {
            Services services = new Services(accounts, sessions, new FileSessionChecks(TestFixtures::standard, () -> runs[0]),
                    TestFixtures::standard, new StandardPipelineValidator(), new StandardBatchGenerator(), runs[0],
                    new FileResultsIndex((who, id) -> runs[0].view(who, id).map(fusion.frontend.runner.RunView::record)),
                    genes, new ExpressionQueries(backend, Duration.ofSeconds(5), data, genes, sessions), backend,
                    new CoalescingFxDispatch(Platform::runLater), p -> { }, new FxFileDialogs());
            Path screens = Path.of("build/screens");
            Parent signIn = onFx(() -> new SignInView(services, a -> { }).root());
            onFx(() -> new Scene(signIn, 1100, 760));
            until(signIn, "the sign-up, with no account yet", () -> hasLabel(signIn, "Create a new account"));
            snapshot(signIn, screens.resolve("sign-up.png"), 1100, 760);

            Account arjun = accounts.create("arjun", "Arjun", "password1".toCharArray());
            Session s = sessions.create(arjun.id(), "EGFR to CXCL8, FSC JUN");
            java.util.Map<String, fusion.frontend.pipeline.InputValue> egf = new java.util.LinkedHashMap<>(
                    TestFixtures.egfSession());
            egf.put("lfc", TestFixtures.path(inputs.resolve("with_node_included.tsv").toString()));   // a values file that exists
            sessions.update(arjun.id(), s.id(), s.version(), x -> x.withInputs(egf));
            Parent main = onFx(() -> new MainView(services, arjun, new Stage(), () -> { }, () -> { }).root());
            onFx(() -> new Scene(main, 1280, 820));
            until(main, "the session list", () -> list(main).getItems().size() == 1);
            onFx(() -> {
                list(main).getSelectionModel().select(0);
                return null;
            });
            until(main, "the session's inputs", () -> hasLabel(main, "Up threshold *") && hasLabel(main, "Backend: fusion 0.1.0 ready"));
            // The classification preview, asked once typing pauses, with each gene's symbol (S8.14).
            until(main, "the classification preview", () -> hasLabel(main, "2 up, 1 down, 0 neither, as cudf classifies them.")
                    && listHolds(main, "hsa1956 EGFR (1.5)") && listHolds(main, "hsa22 TGT22 (-1.3)"));
            onFx(() -> {
                main.lookupAll(".button").stream().map(n -> (javafx.scene.control.Button) n)
                        .filter(b -> b.getText().equals("Suggest thresholds")).findFirst().orElseThrow().fire();
                return null;
            });
            until(main, "the suggestions", () -> main.lookupAll(".table-row-cell").stream()
                    .anyMatch(n -> n instanceof javafx.scene.control.TableRow<?> r && r.getIndex() == 1 && r.getItem() != null));
            snapshot(main, screens.resolve("main-inputs.png"), 1280, 820);
            // One click fills both thresholds (decision 0013); they are kept unsaved until Save or Submit.
            assertTrue(onFx(() -> hasField(main, "1.01") && hasField(main, "-1.01")));
            onFx(() -> {
                main.lookupAll(".table-row-cell").stream().map(n -> (javafx.scene.control.TableRow<?>) n)
                        .filter(r -> r.getIndex() == 1).findFirst().orElseThrow().lookup(".button")
                        .fireEvent(new javafx.event.ActionEvent());
                return null;
            });
            until(main, "the suggestion's thresholds", () -> hasField(main, "1.2") && hasField(main, "-1.3")
                    && !hasField(main, "1.01"));
            assertTrue(Files.size(screens.resolve("main-inputs.png")) > 10_000);
            assertEquals(1, sessions.list(arjun.id()).size());

            // The toy case's inputs, so the run passes the checks; Submit from its inputs, then its Runs tab.
            Session toy = sessions.create(arjun.id(), "Toy hsa1 to hsa22, FSC hsa13");
            java.util.Map<String, fusion.frontend.pipeline.InputValue> values = new java.util.LinkedHashMap<>(
                    TestFixtures.toyPremergedSession());
            values.put("lfc", TestFixtures.path(inputs.resolve("with_node_included.tsv").toString()));
            values.put("merged_graph", TestFixtures.path(inputs.resolve("report_example_with_pathways.xml").toString()));
            values.put("results_dir", TestFixtures.path(tmp.resolve("runs").toString()));
            Session ready = sessions.update(arjun.id(), toy.id(), toy.version(), x -> x.withInputs(values));
            Parent runsScreen = onFx(() -> new MainView(services, arjun, new Stage(), () -> { }, () -> { }).root());
            onFx(() -> new Scene(runsScreen, 1280, 820));
            until(runsScreen, "both sessions", () -> list(runsScreen).getItems().size() == 2);
            onFx(() -> {
                list(runsScreen).getSelectionModel().select(0);       // the newest change: the toy session
                return null;
            });
            until(runsScreen, "the toy session's inputs", () -> hasLabel(runsScreen, "Toy hsa1 to hsa22, FSC hsa13")
                    && hasLabel(runsScreen, "Up threshold *"));
            onFx(() -> {
                runsScreen.lookupAll(".button").stream().map(n -> (javafx.scene.control.Button) n)
                        .filter(b -> b.getText().equals("Submit")).findFirst().orElseThrow().fire();
                return null;
            });
            // fusion --check's warning goes with the submission and the run's view (section 10, Validation).
            until(runsScreen, "the run, its stage, its log and the check's warning", () -> hasLabel(runsScreen, "finished")
                    && runsScreen.lookupAll(".text-area").stream().anyMatch(n -> n instanceof javafx.scene.control.TextArea t
                            && t.getText().contains("Finished: ok"))
                    && labelHolds(runsScreen, "warning: plot_pareto: the font Liberation Sans is missing; gnuplot draws with"
                            + " its default (block plot) [deps.font_missing]")
                    && hasLabel(runsScreen, "Submitted, with 1 warning; the run shows it too."));
            var id = runs[0].runsOf(arjun.id(), ready.id()).get(0).id();
            snapshot(runsScreen, screens.resolve("main-runs.png"), 1280, 820);
            onFx(() -> {
                var tabs = runsScreen.lookupAll(".tab-pane").stream().map(n -> (javafx.scene.control.TabPane) n)
                        .filter(t -> t.getTabs().get(0).getText().equals("Log")).findFirst().orElseThrow();
                tabs.getSelectionModel().select(1);
                return null;
            });
            until(runsScreen, "the frontier", () -> runsScreen.lookupAll(".table-view").stream()
                    .anyMatch(n -> n instanceof javafx.scene.control.TableView<?> t && t.getItems().size() == 3));
            snapshot(runsScreen, screens.resolve("main-runs-frontier.png"), 1280, 820);
            assertEquals(fusion.frontend.runner.RunState.FINISHED, runs[0].view(arjun.id(), id).orElseThrow().record().state());
        } finally {
            runs[0].close();
        }
    }

    /**
     * A real run, for the release notes (poll 333): FuSION 1's toy case {@code toy-hsa13-wni-tsv-b10},
     * submitted from its inputs and run by the backend's staged build on its own home, as
     * {@code RealFusionContractTest} runs it. Drawn: the inputs with fusion's threshold suggestions and
     * histogram, then the finished run's log, frontier, Pareto plot and explanation pages, each checked
     * against the run and against FuSION 1's frontier. Skipped, saying why, without that build.
     */
    @Test
    void aRealRunsViewsDraw() throws Exception {
        Path fusion = TestFixtures.repository().resolve("build/dev/stage/bin/fusion");
        Assumptions.assumeTrue(Files.isExecutable(fusion), "skipped: no fusion at " + fusion
                + "; build the backend (cmake --preset dev, then cmake --build build/dev) to draw a real run");
        startFx();
        Path data = tmp.resolve("data");
        ProcessBackendClient backend = new ProcessBackendClient(fusion, Duration.ofSeconds(60), 4);
        StandardPipelineLoader pipelines = new StandardPipelineLoader(() -> Optional.of(backend.info().home()));
        FileAccountService accounts = new FileAccountService(data, 4, Clock.systemUTC(), UUID::randomUUID);
        DefaultRunService[] runs = new DefaultRunService[1];
        FileSessionStore sessions = new FileSessionStore(data, pipelines, accounts, () -> runs[0], Clock.systemUTC(),
                UUID::randomUUID);
        GeneLookup genes = new GeneLookup(backend, Duration.ofSeconds(60), data);
        runs[0] = new DefaultRunService(new DefaultRunService.Settings(data, tmp.resolve("results"), fusion, 4,
                Duration.ofMillis(100), Duration.ofSeconds(60), FrontendVersion.current()), backend, sessions, accounts,
                pipelines, new StandardPipelineValidator(), new StandardBatchGenerator(), genes, Clock.systemUTC(),
                UUID::randomUUID);
        Path toyCase = TestFixtures.repository().resolve("backend/tests/reference/synthetic/toy-hsa13-wni-tsv-b10");
        try {
            Services services = new Services(accounts, sessions, new FileSessionChecks(pipelines, () -> runs[0]),
                    pipelines, new StandardPipelineValidator(), new StandardBatchGenerator(), runs[0],
                    new FileResultsIndex((who, id) -> runs[0].view(who, id).map(RunView::record)), genes,
                    new ExpressionQueries(backend, Duration.ofSeconds(60), data, genes, sessions), backend,
                    new CoalescingFxDispatch(Platform::runLater), p -> { }, new FxFileDialogs());
            Path screens = Path.of("build/screens");
            Account arjun = accounts.create("arjun", "Arjun", "password1".toCharArray());
            Session created = sessions.create(arjun.id(), "Toy hsa1 to hsa22, FSC hsa13");
            Map<String, InputValue> values = new LinkedHashMap<>(TestFixtures.toyPremergedSession());
            values.put("lfc", new InputValue.PathValue(toyCase.resolve("inputs/with_node_included.tsv")));
            values.put("merged_graph", new InputValue.PathValue(toyCase.resolve("inputs/report_example_with_pathways.xml")));
            values.put("results_dir", new InputValue.PathValue(tmp.resolve("results")));
            Session toy = sessions.update(arjun.id(), created.id(), created.version(), x -> x.withInputs(values));
            Parent main = onFx(() -> new MainView(services, arjun, new Stage(), () -> { }, () -> { }).root());
            onFx(() -> new Scene(main, 1280, 820));
            until(main, "the session list", () -> list(main).getItems().size() == 1);
            onFx(() -> {
                list(main).getSelectionModel().select(0);
                return null;
            });

            // The inputs, scrolled to fusion's threshold suggestions and the histogram of the values (S3.15 to
            // S3.17, decision 0013): the toy file's 22 values are +1 and -1, so ±0.9 keeps 16 up and 6 down.
            until(main, "fusion's classification preview", () -> hasLabel(main,
                    "16 up, 6 down, 0 neither, as cudf classifies them."));
            onFx(() -> {
                fire(main, "Suggest thresholds");
                return null;
            });
            until(main, "fusion's suggestions", () -> table(main, "Criterion").filter(t -> !t.getItems().isEmpty())
                    .isPresent() && main.lookupAll("*").stream().anyMatch(n -> n instanceof Canvas c && c.isVisible()));
            assertEquals(List.of("current", "0.9", "-0.9", "16", "6"),
                    onFx(() -> row(table(main, "Criterion").orElseThrow(), 0).subList(0, 5)));
            onFx(() -> {
                Node histogram = main.lookupAll("*").stream().filter(n -> n instanceof Canvas).findFirst().orElseThrow();
                Node suggestions = table(main, "Criterion").orElseThrow();
                Node up = suggestions;
                while (!(up instanceof ScrollPane)) {
                    up = up.getParent();
                }
                ScrollPane form = (ScrollPane) up;
                Node content = form.getContent();
                Bounds top = content.sceneToLocal(suggestions.localToScene(suggestions.getBoundsInLocal()));
                Bounds bottom = content.sceneToLocal(histogram.localToScene(histogram.getBoundsInLocal()));
                double viewport = form.getViewportBounds().getHeight();
                double range = content.getBoundsInLocal().getHeight() - viewport;
                double y = Math.min(top.getMinY() - 48, bottom.getMaxY() + 12 - viewport);
                form.setVvalue(range <= 0 ? 0 : Math.max(0, Math.min(1, y / range)));
                return null;
            });
            until(main, "the form laid out at its new scroll position", () -> true);
            snapshot(main, screens.resolve("main-inputs-suggestions.png"), 1280, 820);

            // Submit from the inputs; the run is fusion's, and its views fill as it ends.
            onFx(() -> {
                fire(main, "Submit");
                return null;
            });
            Map<String, Set<List<Integer>>> f1 = new TreeMap<>(Map.of(
                    "in", dat(toyCase.resolve("f1/run/hsa1_to_hsa22_w_hsa13_with_node_included.tsv_b10.dat")),
                    "out", dat(toyCase.resolve("f1/run/hsa1_to_hsa22_wo_hsa13_with_node_included.tsv_b10.dat"))));
            // The log lists each case's frontier, as 2.0 found it: FuSION 1's points (S5.25).
            List<String> logLines = new ArrayList<>(f1.values().stream().map(points -> points.stream()
                    .map(p -> "n=" + p.get(0) + " e=" + p.get(1)).collect(Collectors.joining(" | "))).toList());
            logLines.add("Finished: ok, exit 0");
            until(main, "the real run's end and its log", () -> hasLabel(main, "finished") && logHolds(main, logLines));
            RunRecord run = runs[0].runsOf(arjun.id(), toy.id()).get(0);
            assertEquals(RunState.FINISHED, run.state());
            Path dir = run.runDir().orElseThrow();
            snapshot(main, screens.resolve("real-run-log.png"), 1280, 820);

            onFx(() -> {
                details(main).getSelectionModel().select(1);
                return null;
            });
            until(main, "the frontier", () -> table(main, "Case").filter(t -> t.getItems().size() == 8).isPresent());
            List<List<String>> rows = onFx(() -> {
                TableView<?> t = table(main, "Case").orElseThrow();
                List<List<String>> out = new ArrayList<>();
                for (int i = 0; i < t.getItems().size(); i++) {
                    out.add(row(t, i));
                }
                return out;
            });
            Map<String, Set<List<Integer>>> shown = new TreeMap<>();
            for (List<String> r : rows) {
                assertEquals("yes", r.get(3), r::toString);                 // every point certified
                shown.computeIfAbsent(r.get(0), k -> new TreeSet<>(Comparator.<List<Integer>>comparingInt(p -> p.get(0))
                        .thenComparingInt(p -> p.get(1)))).add(List.of(Integer.parseInt(r.get(1)), Integer.parseInt(r.get(2))));
            }
            assertEquals(f1, shown);
            snapshot(main, screens.resolve("real-run-frontier.png"), 1280, 820);

            onFx(() -> {
                details(main).getSelectionModel().select(2);
                return null;
            });
            until(main, "the Pareto plot", () -> main.lookupAll(".image-view").stream().map(n -> (ImageView) n)
                    .anyMatch(v -> v.getImage() != null && v.getImage().getProgress() >= 1.0 && !v.getImage().isError()));
            // The plot shown is the run's own PNG, which section 07 draws at 980 by 690 pixels, fitted whole
            // into its tab.
            Path png = dir.resolve("plot_pareto/pareto.png");
            BufferedImage file = ImageIO.read(png.toFile());
            assertEquals(List.of(png, (double) file.getWidth(), (double) file.getHeight(), true), onFx(() -> {
                ImageView view = main.lookupAll(".image-view").stream().map(n -> (ImageView) n)
                        .filter(v -> v.getImage() != null).findFirst().orElseThrow();
                Node up = view;
                while (!(up instanceof ScrollPane)) {
                    up = up.getParent();
                }
                Bounds tab = ((ScrollPane) up).getViewportBounds();
                Bounds drawn = view.getBoundsInLocal();
                var image = view.getImage();
                return List.<Object>of(Path.of(java.net.URI.create(image.getUrl())), image.getWidth(), image.getHeight(),
                        drawn.getWidth() <= tab.getWidth() && drawn.getHeight() <= tab.getHeight());
            }));
            snapshot(main, screens.resolve("real-run-pareto.png"), 1280, 820);

            onFx(() -> {
                details(main).getSelectionModel().select(3);
                return null;
            });
            // One explanation page per frontier point, named by case and point (1.6).
            Set<String> pages = new TreeSet<>();
            f1.forEach((kase, points) -> points.forEach(p -> pages.add(kase + "_n" + p.get(0) + "_e" + p.get(1) + ".html")));
            until(main, "the explanation pages", () -> main.lookupAll(".list-view").stream().map(n -> (ListView<?>) n)
                    .anyMatch(l -> l.getItems().size() == pages.size() && l.getItems().stream().allMatch(i -> i instanceof Path)));
            assertEquals(pages, onFx(() -> main.lookupAll(".list-view").stream().map(n -> (ListView<?>) n)
                    .filter(l -> !l.getItems().isEmpty() && l.getItems().get(0) instanceof Path).findFirst().orElseThrow()
                    .getItems().stream().map(i -> ((Path) i).getFileName().toString()).collect(Collectors.toCollection(TreeSet::new))));
            for (String page : pages) {
                assertTrue(Files.isRegularFile(dir.resolve("pex/explain").resolve(page)), page);
            }
            snapshot(main, screens.resolve("real-run-explanations.png"), 1280, 820);
        } finally {
            runs[0].close();
        }
    }
}
