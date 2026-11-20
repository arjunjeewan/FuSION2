package fusion.frontend.ui;

import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.function.BiConsumer;
import java.util.function.Function;
import java.util.function.Supplier;

import javafx.animation.PauseTransition;
import javafx.beans.property.ReadOnlyObjectWrapper;
import javafx.beans.property.ReadOnlyStringWrapper;
import javafx.geometry.Insets;
import javafx.scene.Parent;
import javafx.scene.canvas.Canvas;
import javafx.scene.canvas.GraphicsContext;
import javafx.scene.control.Button;
import javafx.scene.control.Label;
import javafx.scene.control.ListView;
import javafx.scene.control.TableCell;
import javafx.scene.control.TableColumn;
import javafx.scene.control.TableView;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Priority;
import javafx.scene.layout.VBox;
import javafx.scene.paint.Color;
import javafx.util.Duration;

import fusion.frontend.pipeline.Catalog;
import fusion.frontend.pipeline.InputValue;
import fusion.frontend.pipeline.PipelineDocument;
import fusion.frontend.runner.ExpressionQueries;

/**
 * The live classification preview (S8.14) and the threshold suggestions (S3.15 to S3.17, decision
 * 0013), both computed by fusion through {@code --query} (04.C8): the thresholds are taken as typed,
 * the preview is asked 250 ms after the last keystroke, and a newer request replaces an older one
 * (section 10, Calling the backend). A suggestion fills both thresholds with one click; the histogram
 * draws the answer's bins (04.C7). FuSION 1 stored the thresholds only when Filter was pressed
 * ({@code Linux/frontend/src/ThresholdFilterPanel.java:235, 434-448}) and previewed with a parser and
 * rules of its own ({@code ThresholdFilterPanel.java:450-520}; S8.14).
 */
final class ThresholdPreview {
    private static final String UNREADABLE = "A threshold above does not read as a number.";
    private final SessionView view;
    private final Catalog catalog;
    private final Supplier<Optional<Map<String, InputValue>>> typed;
    private final ExpressionQueries.Thresholds thresholds;
    private final VBox root = new VBox(8);
    private final Label summary = Ui.muted("Type both thresholds to see how the genes are classified.");
    private final ListView<String> up = new ListView<>();
    private final ListView<String> down = new ListView<>();
    private final TableView<ExpressionQueries.Row> rows = new TableView<>();
    private final Canvas histogram = new Canvas(560, 110);
    private final PauseTransition debounce = new PauseTransition(Duration.millis(250));
    private long requested;

    /**
     * @param typed the form's values as typed, empty while a field does not parse
     * @param fill  sets an input's field to a text, for a chosen suggestion
     */
    ThresholdPreview(SessionView view, Catalog catalog, ExpressionQueries.Thresholds thresholds,
                     Supplier<Optional<Map<String, InputValue>>> typed, BiConsumer<String, String> fill) {
        this.view = view;
        this.catalog = catalog;
        this.thresholds = thresholds;
        this.typed = typed;
        debounce.setOnFinished(e -> request());
        up.setPrefHeight(140);
        down.setPrefHeight(140);
        VBox upBox = new VBox(4, new Label("Up"), up);
        VBox downBox = new VBox(4, new Label("Down"), down);
        HBox.setHgrow(upBox, Priority.ALWAYS);
        HBox.setHgrow(downBox, Priority.ALWAYS);
        Button suggest = new Button("Suggest thresholds");
        suggest.setOnAction(e -> suggest());
        rows.getColumns().add(column("Criterion", ExpressionQueries.Row::criterion));
        rows.getColumns().add(column("Up", r -> InputValue.formatReal(r.up())));
        rows.getColumns().add(column("Down", r -> InputValue.formatReal(r.down())));
        rows.getColumns().add(column("Genes up", r -> Integer.toString(r.nUp())));
        rows.getColumns().add(column("Genes down", r -> Integer.toString(r.nDown())));
        TableColumn<ExpressionQueries.Row, ExpressionQueries.Row> use = new TableColumn<>("");
        use.setCellValueFactory(c -> new ReadOnlyObjectWrapper<>(c.getValue()));
        use.setCellFactory(c -> new TableCell<>() {
            private final Button button = new Button("Use");

            @Override
            protected void updateItem(ExpressionQueries.Row row, boolean empty) {
                super.updateItem(row, empty);
                if (empty || row == null) {
                    setGraphic(null);
                    return;
                }
                button.setOnAction(e -> {
                    thresholds.up().ifPresent(i -> fill.accept(i, InputValue.formatReal(row.up())));
                    thresholds.down().ifPresent(i -> fill.accept(i, InputValue.formatReal(row.down())));
                    schedule();
                });
                setGraphic(button);
            }
        });
        rows.getColumns().add(use);
        rows.setColumnResizePolicy(TableView.CONSTRAINED_RESIZE_POLICY_ALL_COLUMNS);
        rows.setPrefHeight(200);
        rows.setVisible(false);
        rows.setManaged(false);
        histogram.setVisible(false);
        histogram.setManaged(false);
        root.getChildren().addAll(summary, new HBox(12, upBox, downBox), suggest, rows, histogram);
        root.setPadding(new Insets(4));
    }

    Parent root() {
        return root;
    }

    ExpressionQueries.Thresholds thresholds() {
        return thresholds;
    }

    /** Called on each change of the values file or a threshold: asked once typing pauses. */
    void schedule() {
        debounce.playFromStart();
    }

    private void request() {
        Optional<Map<String, InputValue>> values = typed.get();
        if (values.isEmpty()) {
            summary.setText(UNREADABLE);
            return;
        }
        long seq = ++requested;
        var session = view.session();
        PipelineDocument doc = view.document();
        Background.run(() -> view.services().expression().preview(view.account().id(), session, values.get(), doc,
                catalog), preview -> {
                    if (seq == requested) {
                        show(preview);
                    }
                }, e -> {
                    if (seq == requested) {
                        summary.setText(Ui.describe(e));
                        up.getItems().clear();
                        down.getItems().clear();
                    }
                });
    }

    private void show(ExpressionQueries.Preview p) {
        summary.setText(p.up() + " up, " + p.down() + " down, " + p.none() + " neither"
                + (p.skipped().isEmpty() ? "" : "; " + p.skipped().size() + " lines skipped") + ", as cudf classifies them.");
        up.getItems().setAll(names(p, "up"));
        down.getItems().setAll(names(p, "down"));
    }

    private static List<String> names(ExpressionQueries.Preview p, String kind) {
        return p.genes().stream().filter(g -> g.classification().equals(kind))
                .map(g -> g.id() + (g.symbol().isEmpty() ? "" : " " + g.symbol()) + " ("
                        + InputValue.formatReal(g.value()) + ")").toList();
    }

    private void suggest() {
        Optional<Map<String, InputValue>> values = typed.get();
        if (values.isEmpty()) {
            summary.setText(UNREADABLE);
            return;
        }
        var session = view.session();
        PipelineDocument doc = view.document();
        Background.run(() -> view.services().expression().suggest(view.account().id(), session, values.get(), doc,
                catalog), s -> {
                    rows.getItems().setAll(s.rows());
                    rows.setVisible(true);
                    rows.setManaged(true);
                    draw(s);
                }, e -> summary.setText(Ui.describe(e)));
    }

    /** The histogram of the values (04.C7), one bar per bin. */
    private void draw(ExpressionQueries.Suggestions s) {
        histogram.setVisible(!s.counts().isEmpty());
        histogram.setManaged(!s.counts().isEmpty());
        GraphicsContext g = histogram.getGraphicsContext2D();
        g.clearRect(0, 0, histogram.getWidth(), histogram.getHeight());
        int max = s.counts().stream().mapToInt(Integer::intValue).max().orElse(0);
        if (max == 0) {
            return;
        }
        double w = histogram.getWidth() / s.counts().size();
        double h = histogram.getHeight() - 16;
        g.setFill(Color.web("#4a78b5"));
        for (int i = 0; i < s.counts().size(); i++) {
            double bar = h * s.counts().get(i) / max;
            g.fillRect(i * w, h - bar, Math.max(1, w - 1), bar);
        }
        g.setFill(Color.web("#333333"));
        g.fillText(InputValue.formatReal(s.histogramLow()), 0, histogram.getHeight() - 2);
        String hi = InputValue.formatReal(s.histogramLow() + s.histogramWidth() * s.counts().size());
        g.fillText(hi, histogram.getWidth() - 8 * hi.length(), histogram.getHeight() - 2);
    }

    private static TableColumn<ExpressionQueries.Row, String> column(String name,
                                                                  Function<ExpressionQueries.Row, String> f) {
        TableColumn<ExpressionQueries.Row, String> c = new TableColumn<>(name);
        c.setCellValueFactory(cell -> new ReadOnlyStringWrapper(f.apply(cell.getValue())));
        return c;
    }
}
