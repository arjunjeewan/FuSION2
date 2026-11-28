package fusion.frontend.ui;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.ZoneId;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Optional;
import java.util.stream.Stream;

import javafx.beans.property.ReadOnlyStringWrapper;
import javafx.geometry.Insets;
import javafx.scene.Parent;
import javafx.scene.control.Button;
import javafx.scene.control.Label;
import javafx.scene.control.ListCell;
import javafx.scene.control.ListView;
import javafx.scene.control.SplitPane;
import javafx.scene.control.Tab;
import javafx.scene.control.TabPane;
import javafx.scene.control.TableColumn;
import javafx.scene.control.TableView;
import javafx.scene.control.TextArea;
import javafx.scene.image.Image;
import javafx.scene.image.ImageView;
import javafx.scene.layout.BorderPane;
import javafx.scene.layout.HBox;
import javafx.scene.layout.VBox;

import fusion.frontend.accounts.AccountId;
import fusion.frontend.pipeline.Json;
import fusion.frontend.runner.Progress;
import fusion.frontend.runner.RunId;
import fusion.frontend.runner.RunMessage;
import fusion.frontend.runner.RunRecord;
import fusion.frontend.runner.RunState;
import fusion.frontend.runner.RunView;
import tools.jackson.databind.JsonNode;

/**
 * A session's runs (S8.28, S8.29): each with its state, its stage and solve progress from
 * {@code events.jsonl} (A2), its log, Stop, Kill once offered (10.C19) and Resume (decision 0019),
 * and its results: the reported results, the case statuses, the frontier table from
 * {@code results.json} and the Pareto plot as native views, and section 07's run page and
 * explanation pages in the system browser (10.C6). FuSION 1 showed the console alone, ignored the
 * exit status and offered no Stop ({@code Gui4Panel.java:141-246}).
 */
final class RunsPane {
    private static final DateTimeFormatter WHEN = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss", Locale.ROOT)
            .withZone(ZoneId.systemDefault());
    /** The log area keeps the run's newest lines. */
    private static final int LOG_CHARS = 200_000;

    /** One point of a case's frontier. */
    private record Point(String kase, int n, int e, boolean certified) {}

    private record Shown(Optional<RunView> view, boolean killOffered) {}

    private record Results(List<Point> points, Optional<Path> plot, List<Path> pages) {}

    private final SessionView view;
    private final BorderPane root = new BorderPane();
    private final TableView<RunRecord> table = new TableView<>();
    private final Label stage = Ui.title("");
    private final Label progress = Ui.muted("");
    private final Label outcome = Ui.muted("");
    private final TextArea log = new TextArea();
    private final TableView<Point> frontier = new TableView<>();
    private final ImageView plot = new ImageView();
    private final ListView<Path> pages = new ListView<>();
    private final Button stop = new Button("Stop");
    private final Button kill = new Button("Kill");
    private final Button resume = new Button("Resume");
    private final Button folder = new Button("Open folder");
    private final Button runPage = new Button("Open the run page");

    RunsPane(SessionView view) {
        this.view = view;
        table.getColumns().add(column("Submitted", r -> WHEN.format(r.submitted())));
        table.getColumns().add(column("State", r -> r.state().name().toLowerCase(Locale.ROOT)));
        table.getColumns().add(column("Folder", r -> r.runDir().map(p -> p.getFileName().toString()).orElse("")));
        table.getColumns().add(column("Cores", r -> Integer.toString(r.cores())));
        table.getColumns().add(column("Exit", r -> r.osExitStatus().isPresent() ? Integer.toString(r.osExitStatus().getAsInt()) : ""));
        table.setColumnResizePolicy(TableView.CONSTRAINED_RESIZE_POLICY_ALL_COLUMNS);
        table.getSelectionModel().selectedItemProperty().addListener((o, was, now) -> select(now));
        table.setPrefHeight(140);

        stop.setOnAction(e -> act(id -> view.services().runs().stop(who(), id)));
        kill.setOnAction(e -> {
            if (Ui.confirm(view.owner(), "Kill the run?", "Its process and every process it started end now. "
                    + "Every finished solver check is kept, and Resume continues from them.")) {
                act(id -> view.services().runs().kill(who(), id));
            }
        });
        resume.setOnAction(e -> selected().ifPresent(r -> Background.run(() -> view.services().runs().resume(who(), r.id()),
                id -> refresh(), err -> Ui.message(view.owner(), "Not resumed", Ui.describe(err)))));
        folder.setOnAction(e -> selected().flatMap(RunRecord::runDir).ifPresent(p -> view.services().pages().open(p)));
        runPage.setOnAction(e -> selected().ifPresent(r -> Background.run(
                () -> view.services().results().file(who(), r.id(), "index.html"),
                page -> page.ifPresentOrElse(p -> view.services().pages().open(p), () -> {
                    NoRunPage none = NoRunPage.of(r.state());   // why there is none (prompt log entry 387)
                    Ui.message(view.owner(), none.title(), none.text());
                }),
                err -> Ui.message(view.owner(), "Cannot open the run page", Ui.describe(err)))));
        HBox buttons = new HBox(8, stop, kill, resume, folder, runPage);

        log.setEditable(false);
        log.setStyle("-fx-font-family: monospace;");
        frontier.getColumns().add(pointColumn("Case", Point::kase));
        frontier.getColumns().add(pointColumn("Node relaxations", p -> Integer.toString(p.n())));
        frontier.getColumns().add(pointColumn("Edge relaxations", p -> Integer.toString(p.e())));
        frontier.getColumns().add(pointColumn("Certified", p -> p.certified() ? "yes" : "no"));
        frontier.setColumnResizePolicy(TableView.CONSTRAINED_RESIZE_POLICY_ALL_COLUMNS);
        plot.setPreserveRatio(true);
        // The plot fits its tab whole at any window size; gnuplot draws it at 980 by 690 pixels (07).
        javafx.scene.control.ScrollPane plotPane = new javafx.scene.control.ScrollPane(plot);
        plotPane.viewportBoundsProperty().addListener((o, was, now) -> {
            plot.setFitWidth(Math.max(1, now.getWidth() - 2));
            plot.setFitHeight(Math.max(1, now.getHeight() - 2));
        });
        pages.setCellFactory(l -> new ListCell<>() {
            @Override
            protected void updateItem(Path page, boolean empty) {
                super.updateItem(page, empty);
                setText(empty || page == null ? null : page.getFileName().toString());     // named by case and point
            }
        });
        pages.setOnMouseClicked(e -> {
            if (e.getClickCount() == 2 && pages.getSelectionModel().getSelectedItem() != null) {
                view.services().pages().open(pages.getSelectionModel().getSelectedItem());
            }
        });
        TabPane details = new TabPane(tab("Log", log), tab("Frontier", frontier),
                tab("Pareto plot", plotPane),
                tab("Explanation pages", new VBox(4, Ui.muted("Double-click a page to open it in your browser."), pages)));
        outcome.setWrapText(true);                  // Submit's warnings and the results can be long
        VBox top = new VBox(8, table, buttons, stage, progress, outcome);
        top.setPadding(new Insets(12));
        javafx.scene.control.ScrollPane topScroll = new javafx.scene.control.ScrollPane(top);
        topScroll.setFitToWidth(true);
        SplitPane split = new SplitPane(topScroll, details);
        split.setOrientation(javafx.geometry.Orientation.VERTICAL);
        split.setDividerPositions(0.5);
        root.setCenter(split);
        enable(null);
        refresh();
    }

    Parent root() {
        return root;
    }

    private static Tab tab(String name, javafx.scene.Node content) {
        Tab t = new Tab(name, content);
        t.setClosable(false);
        return t;
    }

    private AccountId who() {
        return view.account().id();
    }

    private Optional<RunRecord> selected() {
        return Optional.ofNullable(table.getSelectionModel().getSelectedItem());
    }

    private void act(java.util.function.Consumer<RunId> action) {
        selected().ifPresent(r -> Background.run(() -> {
            action.accept(r.id());
            return r.id();
        }, id -> refresh(), e -> Ui.message(view.owner(), "Not done", Ui.describe(e))));
    }

    void refresh() {
        refresh(Optional.empty());
    }

    /**
     * Runs by submission time, then id (section 10, Determinism); the newest first on screen. The run
     * shown is {@code show}, else the one shown before, else the newest.
     */
    void refresh(Optional<RunId> show) {
        Optional<RunId> keep = show.or(() -> selected().map(RunRecord::id));
        Background.run(() -> {
            List<RunRecord> runs = new ArrayList<>(view.services().runs().runsOf(who(), view.id()));
            java.util.Collections.reverse(runs);
            return runs;
        }, runs -> {
            table.getItems().setAll(runs);
            keep.flatMap(id -> runs.stream().filter(r -> r.id().equals(id)).findFirst())
                    .or(() -> runs.stream().findFirst())
                    .ifPresent(r -> table.getSelectionModel().select(r));
        }, e -> outcome.setText(Ui.describe(e)));
    }

    /** One batch per poll tick (10.C5): a state change refreshes the list, new log text is appended. */
    void onMessages(List<RunMessage> batch) {
        Optional<RunId> current = selected().map(RunRecord::id);
        boolean statuses = batch.stream().anyMatch(m -> m instanceof RunMessage.Status);
        boolean mine = current.isPresent() && batch.stream().anyMatch(m -> m.run().equals(current.get()));
        if (statuses) {
            refresh();
        }
        if (mine) {
            for (RunMessage m : batch) {
                if (m instanceof RunMessage.Log l && l.run().equals(current.get())) {
                    log.appendText(l.text());
                    if (log.getLength() > LOG_CHARS) {
                        log.deleteText(0, log.getLength() - LOG_CHARS);
                    }
                }
            }
            showView(current.get());
        }
    }

    private void select(RunRecord r) {
        enable(r);
        log.clear();
        frontier.getItems().clear();
        plot.setImage(null);
        pages.getItems().clear();
        if (r == null) {
            stage.setText("");
            progress.setText("");
            outcome.setText("");
            return;
        }
        showView(r.id());
        loadLog(r);
        loadResults(r);
    }

    private void showView(RunId id) {
        Background.run(() -> {
            Optional<RunView> v = view.services().runs().view(who(), id);
            return new Shown(v, v.isPresent() && view.services().runs().killOffered(who(), id));
        }, shown -> shown.view().ifPresent(rv -> {
            show(rv);
            kill.setDisable(!shown.killOffered());
        }), e -> outcome.setText(Ui.describe(e)));
    }

    private void show(RunView v) {
        Progress p = v.progress();
        RunRecord r = v.record();
        stage.setText(r.state() == RunState.RUNNING || r.state() == RunState.STOPPING
                ? p.stage().map(s -> s + p.block().map(b -> " (" + b + ")").orElse("")).orElse("Starting")
                : r.state().name().toLowerCase(Locale.ROOT).replace('_', ' '));
        List<String> parts = new ArrayList<>();
        parts.add("elapsed " + p.elapsed().toSeconds() + " s");
        if (p.checks() > 0) {
            parts.add(p.checks() + " solver checks" + (p.unknowns() > 0 ? ", " + p.unknowns() + " unknown" : ""));
        }
        p.pointsByCase().forEach((c, n) -> parts.add(c + ": " + n + " Pareto point" + (n == 1 ? "" : "s")));
        p.checking().ifPresent(c -> parts.add("checking " + c));
        progress.setText(String.join(" · ", parts));
        List<String> out = new ArrayList<>();
        // Submit's warnings: shown, and they did not block (section 10, Validation).
        v.warnings().forEach(w -> out.add(w.toString()));
        v.caseStatuses().forEach((c, s) -> out.add("case " + c + ": " + s));
        for (JsonNode rep : v.reports()) {
            out.add("result " + rep.path("code").asString("") + ": " + rep.path("message").asString(""));
        }
        v.failure().ifPresent(f -> out.add("failure " + f.path("code").asString("") + ": " + f.path("message").asString("")));
        outcome.setText(String.join("\n", out));
    }

    private void enable(RunRecord r) {
        RunState s = r == null ? null : r.state();
        stop.setDisable(s == null || !(s == RunState.QUEUED || s == RunState.STARTING || s == RunState.RUNNING));
        kill.setDisable(true);
        resume.setDisable(s == null || !s.isResumable());
        folder.setDisable(r == null || r.runDir().isEmpty());
        runPage.setDisable(r == null || r.runDir().isEmpty());
    }

    /** The run log from the start: a finished run's 20 to 52 KB (10.C5). */
    private void loadLog(RunRecord r) {
        if (r.runDir().isEmpty()) {
            return;
        }
        String name = r.runDir().get().getFileName() + ".log";
        Background.run(() -> view.services().results().file(who(), r.id(), name).map(p -> {
            try {
                String text = Files.readString(p);
                return text.length() > LOG_CHARS ? text.substring(text.length() - LOG_CHARS) : text;
            } catch (IOException e) {
                return "";
            }
        }).orElse(""), text -> {
            if (selected().map(RunRecord::id).filter(r.id()::equals).isPresent()) {
                log.setText(text);
            }
        }, e -> { });
    }

    /** Section 07's results: the frontier of results.json, the Pareto plot and the pages. */
    private void loadResults(RunRecord r) {
        if (r.runDir().isEmpty()) {
            return;
        }
        Background.run(() -> {
            List<Point> points = new ArrayList<>();
            view.services().results().file(who(), r.id(), "pathz3/results.json").ifPresent(f -> {
                try {
                    JsonNode results = Json.parse(Files.readString(f), f.toString());
                    for (JsonNode c : results.path("cases")) {
                        for (JsonNode p : c.path("points")) {
                            points.add(new Point(c.path("case").asString("?"), p.path("n").asInt(), p.path("e").asInt(),
                                    p.path("certified").asBoolean(false)));
                        }
                    }
                } catch (IOException | RuntimeException e) {
                    // shown as an empty table
                }
            });
            Optional<Path> png = view.services().results().file(who(), r.id(), "plot_pareto/pareto.png");
            List<Path> html = new ArrayList<>();
            view.services().results().file(who(), r.id(), "pex/explain").ifPresent(dir -> {
                try (Stream<Path> files = Files.list(dir)) {
                    files.filter(p -> p.getFileName().toString().endsWith(".html")).sorted().forEach(html::add);
                } catch (IOException e) {
                    // no pages
                }
            });
            return new Results(points, png, html);
        }, found -> {
            if (selected().map(RunRecord::id).filter(r.id()::equals).isEmpty()) {
                return;
            }
            frontier.getItems().setAll(found.points());
            found.plot().ifPresent(p -> plot.setImage(new Image(p.toUri().toString(), true)));
            pages.getItems().setAll(found.pages());
        }, e -> outcome.setText(Ui.describe(e)));
    }

    private static TableColumn<RunRecord, String> column(String name, java.util.function.Function<RunRecord, String> f) {
        TableColumn<RunRecord, String> c = new TableColumn<>(name);
        c.setCellValueFactory(cell -> new ReadOnlyStringWrapper(f.apply(cell.getValue())));
        return c;
    }

    private static TableColumn<Point, String> pointColumn(String name, java.util.function.Function<Point, String> f) {
        TableColumn<Point, String> c = new TableColumn<>(name);
        c.setCellValueFactory(cell -> new ReadOnlyStringWrapper(f.apply(cell.getValue())));
        return c;
    }
}
