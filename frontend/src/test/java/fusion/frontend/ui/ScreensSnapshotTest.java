package fusion.frontend.ui;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.awt.image.BufferedImage;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Clock;
import java.time.Duration;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.OptionalInt;
import java.util.UUID;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.TimeUnit;
import java.util.function.Supplier;

import javax.imageio.ImageIO;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.condition.EnabledIfEnvironmentVariable;
import org.junit.jupiter.api.io.TempDir;

import javafx.application.Platform;
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.scene.image.PixelReader;
import javafx.scene.image.WritableImage;
import javafx.stage.Stage;

import fusion.frontend.accounts.Account;
import fusion.frontend.accounts.FileAccountService;
import fusion.frontend.pipeline.Catalog;
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
import fusion.frontend.sessions.FileSessionChecks;
import fusion.frontend.sessions.FileSessionStore;
import fusion.frontend.sessions.Session;
import tools.jackson.databind.JsonNode;

/**
 * The screens build and draw (Phase 5 designs and tests them; this checks the minimal ones of
 * decision 0038). Off-screen: no window is shown; each scene is drawn into
 * {@code build/screens/*.png}. It needs a display, which CI's Xvfb gives (11, ci.yml).
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
}
