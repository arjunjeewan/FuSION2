package fusion.frontend.ui;

import java.nio.file.InvalidPathException;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.regex.Pattern;
import java.util.stream.Stream;

import javafx.collections.FXCollections;
import javafx.geometry.Insets;
import javafx.scene.Node;
import javafx.scene.Parent;
import javafx.scene.control.Button;
import javafx.scene.control.CheckBox;
import javafx.scene.control.ComboBox;
import javafx.scene.control.Label;
import javafx.scene.control.ListView;
import javafx.scene.control.ScrollPane;
import javafx.scene.control.TextField;
import javafx.scene.control.TitledPane;
import javafx.scene.control.Tooltip;
import javafx.scene.layout.BorderPane;
import javafx.scene.layout.GridPane;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Priority;
import javafx.scene.layout.VBox;

import fusion.frontend.accounts.Account;
import fusion.frontend.accounts.Preferences;
import fusion.frontend.pipeline.ArgType;
import fusion.frontend.pipeline.Binding;
import fusion.frontend.pipeline.Block;
import fusion.frontend.pipeline.Catalog;
import fusion.frontend.pipeline.InputDecl;
import fusion.frontend.pipeline.InputHint;
import fusion.frontend.pipeline.InputValue;
import fusion.frontend.pipeline.PipelineDocument;
import fusion.frontend.pipeline.PipelineProblem;
import fusion.frontend.runner.ExpressionQueries;
import fusion.frontend.runner.SubmitRequest;
import fusion.frontend.sessions.PathProblem;

/**
 * A session's inputs, one form built from the pipeline's declarations (S8.07 to S8.16), grouped as
 * the pipeline groups them. Paths can be typed, pasted or chosen in a native dialog, and both pass the
 * same checks (10.C7, S8.30). A required input with no default offers the backend's value as a
 * one-click suggestion (10.C13). Values that do not parse are refused, never stored as 0
 * ({@code ReachPathBoundCard.java:153-163}; S8.10); typed values need no Enter
 * ({@code ReachPathBoundCard.java:17-67}, {@code ThresholdFilterPanel.java:167-201}; S8.09, S8.14).
 * Below the classification's inputs, fusion previews the classification as typed and suggests
 * thresholds ({@link ThresholdPreview}; S8.14, S3.15 to S3.17).
 */
final class InputsPane {
    /** 04.C2's one decimal grammar, so a form never takes what fusion would refuse. */
    private static final Pattern DECIMAL = Pattern.compile("[+-]?([0-9]+(\\.[0-9]*)?|\\.[0-9]+)([eE][+-]?[0-9]+)?");
    private static final Pattern INTEGER = Pattern.compile("[+-]?[0-9]+");
    private static final String HOME = "$FUSION_HOME/";

    private final SessionView view;
    private final BorderPane root = new BorderPane();
    private final Map<String, Field> fields = new LinkedHashMap<>();
    private final ListView<String> problems = new ListView<>();
    private final Label status = Ui.muted("");
    private final Optional<ThresholdPreview> preview;

    InputsPane(SessionView view) {
        this.view = view;
        VBox form = new VBox(8);
        form.setPadding(new Insets(12));
        Map<String, GridPane> groups = new LinkedHashMap<>();
        PipelineDocument doc = view.document();
        for (InputDecl d : doc.inputs()) {
            GridPane grid = groups.computeIfAbsent(group(d), g -> {
                GridPane gp = new GridPane();
                gp.setHgap(10);
                gp.setVgap(6);
                return gp;
            });
            Field f = new Field(d, suggestion(doc, d, view.catalog()));
            fields.put(d.name(), f);
            int row = grid.getRowCount();
            Label label = new Label(d.label() + (d.required() ? " *" : "") + d.unit().map(u -> " (" + u + ")").orElse(""));
            if (!d.help().isEmpty()) {
                label.setTooltip(new Tooltip(d.help()));
            }
            grid.add(label, 0, row);
            grid.add(f.node, 1, row);
            GridPane.setHgrow(f.node, Priority.ALWAYS);
        }
        preview = preview(doc);
        Optional<String> previewAfter = preview.map(p -> group(fields.get(p.thresholds().up().orElseThrow()).decl));
        groups.forEach((name, grid) -> {
            TitledPane p = new TitledPane(name, grid);
            p.setCollapsible(true);
            form.getChildren().add(p);
            if (previewAfter.filter(name::equals).isPresent()) {
                form.getChildren().add(new TitledPane("Classification preview", preview.orElseThrow().root()));
            }
        });
        ScrollPane scroll = new ScrollPane(form);
        scroll.setFitToWidth(true);
        root.setCenter(scroll);

        Button save = new Button("Save");
        save.setOnAction(e -> save(() -> status.setText("Saved.")));
        Button check = new Button("Check");
        check.setOnAction(e -> save(this::check));
        Button submit = new Button("Submit");
        submit.setDefaultButton(true);
        submit.setOnAction(e -> save(() -> {
            status.setText("Checking and submitting...");
            view.submit(SubmitRequest.QUICK, this::showProblems);
        }));
        problems.setPrefHeight(150);
        VBox bottom = new VBox(6, new HBox(8, save, check, submit, status), problems);
        bottom.setPadding(new Insets(8, 12, 12, 12));
        root.setBottom(bottom);
        refresh();
    }

    Parent root() {
        return root;
    }

    private static String group(InputDecl d) {
        return d.group().isEmpty() ? "Other" : d.group();
    }

    void refresh() {
        Map<String, InputValue> values = view.session().inputs();
        for (Field f : fields.values()) {
            f.show(values.getOrDefault(f.decl.name(), f.decl.initialValue()));
        }
        preview.ifPresent(ThresholdPreview::schedule);
    }

    /**
     * The preview, when the pipeline classifies genes with inputs this form shows; it is asked again
     * whenever one of them is edited, and on every refresh, which a change of the Not DE list makes.
     */
    private Optional<ThresholdPreview> preview(PipelineDocument doc) {
        Optional<Catalog> catalog = view.catalog();
        Optional<ExpressionQueries.Thresholds> found = catalog
                .map(c -> view.services().expression().thresholds(doc, view.session().inputs(), c))
                .filter(t -> Stream.of(t.values(), t.up(), t.down()).allMatch(i -> i.filter(fields::containsKey).isPresent()));
        return found.map(t -> {
            ThresholdPreview p = new ThresholdPreview(view, catalog.orElseThrow(), t, () -> typed(t),
                    (input, text) -> fields.get(input).fill(text));
            Stream.of(t.values(), t.up(), t.down()).forEach(i -> fields.get(i.orElseThrow()).onEdit(p::schedule));
            return p;
        });
    }

    /**
     * The values as typed, unsaved: a field that does not parse keeps its stored value, unless the
     * preview sends it, and then there is no preview until it parses.
     */
    private Optional<Map<String, InputValue>> typed(ExpressionQueries.Thresholds t) {
        Map<String, InputValue> values = new LinkedHashMap<>(view.session().inputs());
        for (Field f : fields.values()) {
            try {
                values.put(f.decl.name(), f.read());
            } catch (IllegalArgumentException e) {
                if (Stream.of(t.values(), t.up(), t.down()).anyMatch(i -> i.filter(f.decl.name()::equals).isPresent())) {
                    return Optional.empty();
                }
            }
        }
        return Optional.of(values);
    }

    /** The backend's default of the argument an input is bound to, as section 06's values (10.C13). */
    private static Optional<String> suggestion(PipelineDocument doc, InputDecl d, Optional<Catalog> catalog) {
        if (!d.required() || d.defaultValue().isPresent() || catalog.isEmpty()) {
            return Optional.empty();
        }
        for (Block b : doc.blocks()) {
            for (Map.Entry<String, Binding> e : b.args().entrySet()) {
                if (names(e.getValue(), d.name())) {
                    return catalog.get().command(b.command()).flatMap(c -> c.arg(e.getKey()))
                            .flatMap(a -> a.defaultValue());
                }
            }
        }
        return Optional.empty();
    }

    private static boolean names(Binding b, String input) {
        if (b instanceof Binding.Input in) {
            return in.name().equals(input);
        }
        return b instanceof Binding.FirstOf f && f.options().stream().anyMatch(o -> names(o, input));
    }

    // ---- saving and checking --------------------------------------------------------------------

    private void save(Runnable then) {
        Map<String, InputValue> values = new LinkedHashMap<>(view.session().inputs());
        List<String> bad = new ArrayList<>();
        for (Field f : fields.values()) {
            try {
                values.put(f.decl.name(), f.read());
            } catch (IllegalArgumentException e) {
                bad.add(f.decl.label() + ": " + e.getMessage());
            }
        }
        if (!bad.isEmpty()) {
            problems.getItems().setAll(bad);
            status.setText("Nothing was saved; fix the values marked below.");
            return;
        }
        if (values.equals(view.session().inputs())) {
            then.run();
            return;
        }
        view.change(s -> s.withInputs(values), s -> {
            remember(values);
            then.run();
        });
    }

    /** New sessions copy the inputs marked remember, such as the KGML folder (S8.31). */
    private void remember(Map<String, InputValue> values) {
        Account account = view.account();
        List<InputDecl> decls = view.document().inputs();
        Background.run(() -> {
            Preferences p = view.services().accounts().find(account.id()).map(Account::preferences)
                    .orElse(Preferences.NONE);
            Preferences next = p;
            for (InputDecl d : decls) {
                InputValue v = values.getOrDefault(d.name(), InputValue.UNSET);
                if (d.remember() && v.isGiven() && !v.equals(p.rememberedInputs().get(d.name()))) {
                    next = next.remember(d.name(), v);
                }
            }
            if (next != p) {
                view.services().accounts().setPreferences(account.id(), next);
            }
            return next;
        }, p -> { }, e -> status.setText(Ui.describe(e)));
    }

    private void check() {
        status.setText("Checking...");
        fusion.frontend.sessions.Session session = view.session();
        PipelineDocument doc = view.document();
        Optional<Catalog> catalog = view.catalog();
        Background.run(() -> {
            List<String> out = new ArrayList<>();
            for (PathProblem p : view.services().checks().quick(session)) {
                out.add(p.kind().name().toLowerCase(java.util.Locale.ROOT).replace('_', ' ') + ": " + p.input() + " "
                        + p.path());
            }
            if (catalog.isPresent()) {
                for (PipelineProblem p : view.services().validator().validate(doc, session.inputs(), catalog.get())) {
                    out.add(p.toString());
                }
            } else {
                out.add("warning: the backend is not available, so only the paths were checked");
            }
            return out;
        }, out -> {
            problems.getItems().setAll(out.isEmpty() ? List.of("No problem found.") : out);
            status.setText("");
        }, e -> status.setText(Ui.describe(e)));
    }

    /** Submit's errors when it was refused, else its warnings, which do not block (section 10, Validation). */
    private void showProblems(List<PipelineProblem> found) {
        status.setText(found.stream().anyMatch(PipelineProblem::isError) ? "The run was not submitted."
                : SessionView.submitted(found));
        problems.getItems().setAll(found.stream().map(PipelineProblem::toString).toList());
    }

    // ---- one field ------------------------------------------------------------------------------

    private final class Field {
        final InputDecl decl;
        final Node node;
        private TextField text;
        private CheckBox flag;
        private ComboBox<String> choice;
        private CheckBox sweep;

        Field(InputDecl decl, Optional<String> suggested) {
            this.decl = decl;
            HBox box = new HBox(6);
            if (decl.type() == ArgType.BOOLEAN) {
                flag = new CheckBox();
                box.getChildren().add(flag);
            } else if (decl.type() == ArgType.CHOICE && !decl.choices().isEmpty()) {
                choice = new ComboBox<>(FXCollections.observableArrayList(decl.choices()));
                box.getChildren().add(choice);
            } else {
                text = new TextField();
                HBox.setHgrow(text, Priority.ALWAYS);
                text.setPromptText(prompt());
                box.getChildren().add(text);
                if (decl.type().isPath()) {
                    Button browse = new Button("Browse...");
                    browse.setOnAction(e -> browse());
                    box.getChildren().add(browse);
                }
            }
            if (decl.sweepable()) {
                sweep = new CheckBox("sweep");
                sweep.setTooltip(new Tooltip("Run once per value: give several values, separated by commas"));
                box.getChildren().add(sweep);
            }
            suggested.ifPresent(s -> {
                Button use = new Button("Use " + s);
                use.setTooltip(new Tooltip("fusion's own default for this setting, offered as a suggestion"));
                use.setOnAction(e -> {
                    if (text != null) {
                        text.setText(s);
                    }
                });
                box.getChildren().add(use);
            });
            node = box;
        }

        /** Calls back on every edit, typed or chosen. */
        void onEdit(Runnable r) {
            if (text != null) {
                text.textProperty().addListener((o, a, b) -> r.run());
            }
            if (flag != null) {
                flag.selectedProperty().addListener((o, a, b) -> r.run());
            }
            if (choice != null) {
                choice.valueProperty().addListener((o, a, b) -> r.run());
            }
            if (sweep != null) {
                sweep.selectedProperty().addListener((o, a, b) -> r.run());
            }
        }

        /** Sets a typed field's text, for a suggestion taken with one click. */
        void fill(String t) {
            if (text != null) {
                text.setText(t);
            }
        }

        private String prompt() {
            return switch (decl.type()) {
                case DURATION -> "a duration with a unit: 90s, 50min, 4h";
                case REAL -> "a number, such as 1.01";
                case INTEGER -> "a whole number";
                case INPUT_FILE, INPUT_LIST -> decl.hint().filter(h -> h == InputHint.DIRECTORY).isPresent()
                        ? "a folder" : "a file";
                case OUTPUT_NAME -> "a folder";
                default -> decl.list() ? "one or more, separated by commas" : "";
            };
        }

        private void browse() {
            Optional<Path> current = currentPath();
            boolean folder = decl.hint().filter(h -> h == InputHint.DIRECTORY).isPresent()
                    || decl.type() == ArgType.OUTPUT_NAME;
            Optional<Path> chosen = folder
                    ? view.services().dialogs().chooseDirectory(view.owner(), decl.label(), current)
                    : view.services().dialogs().chooseFile(view.owner(), decl.label(), current, List.of());
            chosen.ifPresent(p -> text.setText(p.toAbsolutePath().toString()));
        }

        private Optional<Path> currentPath() {
            try {
                String t = text.getText().strip();
                return t.isEmpty() || t.startsWith(HOME) ? Optional.empty() : Optional.of(Path.of(t));
            } catch (InvalidPathException e) {
                return Optional.empty();
            }
        }

        void show(InputValue v) {
            if (flag != null) {
                flag.setSelected(v instanceof InputValue.Flag f && f.value());
            } else if (choice != null) {
                choice.setValue(v.isSet() ? v.text() : null);
            } else {
                boolean swept = v instanceof InputValue.Sweep;
                if (sweep != null) {
                    sweep.setSelected(swept);
                }
                text.setText(display(v));
            }
        }

        private String display(InputValue v) {
            return switch (v) {
                case InputValue.Unset u -> "";
                case InputValue.InstallPath i -> HOME + i.relative();
                case InputValue.Items l -> String.join(", ", l.items().stream().map(this::display).toList());
                case InputValue.Sweep s -> String.join(", ", s.values().stream().map(this::display).toList());
                default -> v.text();
            };
        }

        InputValue read() {
            if (flag != null) {
                return new InputValue.Flag(flag.isSelected());
            }
            if (choice != null) {
                return choice.getValue() == null ? InputValue.UNSET : new InputValue.Choice(choice.getValue());
            }
            String t = text.getText().strip();
            if (t.isEmpty()) {
                return InputValue.UNSET;
            }
            if (sweep != null && sweep.isSelected()) {
                List<InputValue> values = new ArrayList<>();
                for (String part : t.split(",")) {
                    if (!part.isBlank()) {
                        values.add(scalar(part.strip()));
                    }
                }
                return new InputValue.Sweep(values);
            }
            if (decl.list()) {
                List<InputValue> items = new ArrayList<>();
                for (String part : t.split(",")) {
                    if (!part.isBlank()) {
                        items.add(scalar(part.strip()));
                    }
                }
                return items.isEmpty() ? InputValue.UNSET : new InputValue.Items(items);
            }
            return scalar(t);
        }

        private InputValue scalar(String t) {
            return switch (decl.type()) {
                case INTEGER -> {
                    if (!INTEGER.matcher(t).matches()) {
                        throw new IllegalArgumentException("'" + t + "' is not a whole number");
                    }
                    yield new InputValue.Int(Long.parseLong(t));
                }
                case REAL -> {
                    if (!DECIMAL.matcher(t).matches()) {
                        throw new IllegalArgumentException("'" + t + "' is not a number such as 1.01");
                    }
                    yield new InputValue.Real(Double.parseDouble(t));
                }
                case TEXT -> decl.hint().filter(h -> h == InputHint.GENE_ID).isPresent()
                        ? new InputValue.GeneId(t) : new InputValue.Text(t);
                case CHOICE -> new InputValue.Choice(t);
                case INPUT_FILE, INPUT_LIST, OUTPUT_NAME -> {
                    if (t.startsWith(HOME)) {
                        yield new InputValue.InstallPath(t.substring(HOME.length()));
                    }
                    try {
                        yield new InputValue.PathValue(Path.of(t));
                    } catch (InvalidPathException e) {
                        throw new IllegalArgumentException("'" + t + "' is not a path");
                    }
                }
                default -> new InputValue.Text(t);
            };
        }
    }
}
