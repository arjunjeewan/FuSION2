package fusion.frontend.ui;

import java.util.ArrayList;
import java.util.List;
import java.util.Optional;

import javafx.beans.property.ReadOnlyStringWrapper;
import javafx.collections.FXCollections;
import javafx.geometry.Insets;
import javafx.scene.Parent;
import javafx.scene.control.Button;
import javafx.scene.control.ComboBox;
import javafx.scene.control.ContextMenu;
import javafx.scene.control.Label;
import javafx.scene.control.MenuItem;
import javafx.scene.control.TableColumn;
import javafx.scene.control.TableView;
import javafx.scene.control.TextField;
import javafx.scene.layout.BorderPane;
import javafx.scene.layout.HBox;
import javafx.scene.layout.VBox;

import fusion.frontend.pipeline.Activity;
import fusion.frontend.pipeline.GeneOverride;
import fusion.frontend.pipeline.Observation;
import fusion.frontend.pipeline.Relaxation;
import fusion.frontend.pipeline.Wire;
import fusion.frontend.runner.GeneLookup;

/**
 * A session's gene overrides (S8.15, entry 165): for each gene, at most one of Above Up, Not DE or
 * Below Down, one of Active or Inactive, and Non-relaxed. They are stored with the session and
 * written as one-id-per-line files for a run. Genes are bare ids, found through the symbol-aware
 * lookup (S8.12, 10.C15). FuSION 1 kept overrides as display text that never reached the solve, and
 * a Search alone could record one ({@code ThresholdFilterPanel.java:247-404, 522-578}).
 */
final class OverridesPane {
    private static final String NONE = "(none)";

    private final SessionView view;
    private final BorderPane root = new BorderPane();
    private final TableView<GeneOverride> table = new TableView<>();
    private final TextField gene = new TextField();
    private final ComboBox<String> observation = combo(Observation.values());
    private final ComboBox<String> activity = combo(Activity.values());
    private final ComboBox<String> relaxation = combo(Relaxation.values());
    private final Label status = Ui.muted("");
    private final ContextMenu suggestions = new ContextMenu();

    OverridesPane(SessionView view) {
        this.view = view;
        table.getColumns().add(column("Gene", g -> g.geneId()));
        table.getColumns().add(column("Observation", g -> g.observation().map(Wire::name).orElse("")));
        table.getColumns().add(column("Activity", g -> g.activity().map(Wire::name).orElse("")));
        table.getColumns().add(column("Relaxation", g -> g.relaxation().map(Wire::name).orElse("")));
        table.setColumnResizePolicy(TableView.CONSTRAINED_RESIZE_POLICY_ALL_COLUMNS);
        table.getSelectionModel().selectedItemProperty().addListener((o, was, g) -> {
            if (g != null) {
                gene.setText(g.geneId());
                observation.setValue(g.observation().map(Wire::name).orElse(NONE));
                activity.setValue(g.activity().map(Wire::name).orElse(NONE));
                relaxation.setValue(g.relaxation().map(Wire::name).orElse(NONE));
            }
        });
        gene.setPromptText("gene id or symbol");
        gene.textProperty().addListener((o, was, now) -> lookup(now));
        Button set = new Button("Set");
        set.setOnAction(e -> set());
        Button remove = new Button("Remove");
        remove.setOnAction(e -> remove());
        VBox top = new VBox(6, Ui.muted("Not DE takes a gene out of the observed up and down lists; the other kinds "
                + "go to the solve's lists of the same names, and confirmed genes are also kept apart when merging."),
                new HBox(6, gene, observation, activity, relaxation, set, remove), status);
        top.setPadding(new Insets(12));
        root.setTop(top);
        root.setCenter(table);
        refresh();
    }

    Parent root() {
        return root;
    }

    void refresh() {
        table.getItems().setAll(view.session().overrides());
    }

    private static <E extends Enum<E>> ComboBox<String> combo(E[] values) {
        List<String> names = new ArrayList<>();
        names.add(NONE);
        for (E v : values) {
            names.add(Wire.name(v));
        }
        ComboBox<String> c = new ComboBox<>(FXCollections.observableArrayList(names));
        c.setValue(NONE);
        return c;
    }

    private static TableColumn<GeneOverride, String> column(String name, java.util.function.Function<GeneOverride, String> f) {
        TableColumn<GeneOverride, String> c = new TableColumn<>(name);
        c.setCellValueFactory(cell -> new ReadOnlyStringWrapper(f.apply(cell.getValue())));
        return c;
    }

    private static <E extends Enum<E>> Optional<E> pick(Class<E> type, String value) {
        return value == null || value.equals(NONE) ? Optional.empty() : Optional.of(Wire.parse(type, value, "override"));
    }

    /** One row per gene: setting a gene again replaces its row (S8.15). */
    private void set() {
        String id = gene.getText().strip();
        if (id.isEmpty()) {
            status.setText("Give a gene id.");
            return;
        }
        GeneOverride next = new GeneOverride(id, pick(Observation.class, observation.getValue()),
                pick(Activity.class, activity.getValue()), pick(Relaxation.class, relaxation.getValue()));
        List<GeneOverride> rows = new ArrayList<>(view.session().overrides());
        rows.removeIf(g -> g.geneId().equals(id));
        if (!next.isEmpty()) {
            rows.add(next);
        }
        view.change(s -> s.withOverrides(rows), s -> status.setText("Saved."));
    }

    private void remove() {
        GeneOverride selected = table.getSelectionModel().getSelectedItem();
        if (selected == null) {
            return;
        }
        List<GeneOverride> rows = new ArrayList<>(view.session().overrides());
        rows.remove(selected);
        view.change(s -> s.withOverrides(rows), s -> status.setText("Removed " + selected.geneId() + "."));
    }

    /** Suggestions from the symbol map, exact matches first, then prefixes, then substrings (10.C15). */
    private void lookup(String text) {
        if (text == null || text.strip().length() < 2) {
            suggestions.hide();
            return;
        }
        String query = text.strip();
        Background.run(() -> view.services().genes().search(Optional.empty(), query), (List<GeneLookup.Gene> found) -> {
            if (!gene.getText().strip().equals(query) || found.isEmpty()
                    || found.size() == 1 && found.get(0).id().equals(query)) {
                suggestions.hide();
                return;
            }
            suggestions.getItems().setAll(found.stream().map(g -> {
                MenuItem item = new MenuItem(g.id() + "  " + g.label());
                item.setOnAction(e -> gene.setText(g.id()));
                return item;
            }).toList());
            if (!suggestions.isShowing() && gene.getScene() != null) {
                suggestions.show(gene, javafx.geometry.Side.BOTTOM, 0, 0);
            }
        }, e -> suggestions.hide());
    }
}
