package fusion.frontend.ui;

import java.util.List;
import java.util.Optional;
import java.util.function.Consumer;
import java.util.function.UnaryOperator;

import javafx.scene.Parent;
import javafx.scene.control.Label;
import javafx.scene.control.Tab;
import javafx.scene.control.TabPane;
import javafx.scene.layout.BorderPane;
import javafx.scene.layout.VBox;
import javafx.stage.Window;

import fusion.frontend.accounts.Account;
import fusion.frontend.pipeline.Catalog;
import fusion.frontend.pipeline.PipelineDocument;
import fusion.frontend.pipeline.PipelineProblem;
import fusion.frontend.runner.RunId;
import fusion.frontend.runner.RunMessage;
import fusion.frontend.runner.RunnerException;
import fusion.frontend.runner.SubmitRequest;
import fusion.frontend.sessions.Session;
import fusion.frontend.sessions.SessionException;
import fusion.frontend.sessions.SessionId;

/**
 * One open session (S8.04, S8.05): its inputs, its gene overrides, its pipeline and its runs. Every
 * change states the version it was based on, so two screens never overwrite each other silently
 * (10.C8); a conflict reloads the session and says so.
 */
final class SessionView {
    private final Services services;
    private final Account account;
    private final Window owner;
    private final Runnable onListChanged;
    private final BorderPane root = new BorderPane();
    private final TabPane tabs = new TabPane();
    private final Label header = Ui.title("");
    private final Label note = Ui.muted("");
    private Session session;
    private PipelineDocument standard;
    private Optional<Catalog> catalog = Optional.empty();
    private InputsPane inputs;
    private OverridesPane overrides;
    private PipelinePane pipeline;
    private RunsPane runs;

    private record Loaded(PipelineDocument standard, Optional<Catalog> catalog) {}

    /** What a Submit made: its runs, and the warnings their checks gave. */
    private record Submitted(List<RunId> runs, List<PipelineProblem> warnings) {}

    SessionView(Services services, Account account, Session session, Window owner, Runnable onListChanged) {
        this.services = services;
        this.account = account;
        this.session = session;
        this.owner = owner;
        this.onListChanged = onListChanged;
        header.setText(session.name());
        root.setTop(Ui.padded(new VBox(4, header, note)));
        root.setCenter(Ui.padded(new VBox(Ui.muted("Loading the pipeline..."))));
        Background.run(() -> {
            PipelineDocument std = services.pipelines().standard();
            Optional<Catalog> cat;
            try {
                cat = Optional.of(services.backend().catalog());
            } catch (RuntimeException e) {
                cat = Optional.empty();
            }
            return new Loaded(std, cat);
        }, this::build, e -> root.setCenter(Ui.padded(new VBox(Ui.error(),
                Ui.muted("The default pipeline is not available: " + Ui.describe(e)
                        + "\nSet fusion.backend.executable, or FUSION_HOME, and start FuSION again.")))));
    }

    private void build(Loaded loaded) {
        standard = loaded.standard();
        catalog = loaded.catalog();
        inputs = new InputsPane(this);
        overrides = new OverridesPane(this);
        pipeline = new PipelinePane(this);
        runs = new RunsPane(this);
        tabs.getTabs().setAll(tab("Inputs", inputs.root()), tab("Overrides", overrides.root()),
                tab("Pipeline", pipeline.root()), tab("Runs", runs.root()));
        root.setCenter(tabs);
        showNote();
    }

    private static Tab tab(String name, Parent content) {
        Tab t = new Tab(name, content);
        t.setClosable(false);
        return t;
    }

    private void showNote() {
        note.setText(session.pipeline().map(p -> "Runs an edited copy of " + p.origin())
                .orElse("Runs the default pipeline, " + standard.origin()) + " · version " + session.version());
    }

    SessionId id() {
        return session.id();
    }

    Parent root() {
        return root;
    }

    Session session() {
        return session;
    }

    Services services() {
        return services;
    }

    Account account() {
        return account;
    }

    Window owner() {
        return owner;
    }

    PipelineDocument standard() {
        return standard;
    }

    /** The pipeline the session runs: its edited copy, or the default (entry 195). */
    PipelineDocument document() {
        return session.pipeline().orElse(standard);
    }

    Optional<Catalog> catalog() {
        return catalog;
    }

    /** One change, based on the version shown; the panes refresh from the stored result. */
    void change(UnaryOperator<Session> change, Consumer<Session> after) {
        long version = session.version();
        Background.run(() -> services.sessions().update(account.id(), session.id(), version, change), s -> {
            session = s;
            refresh();
            onListChanged.run();
            after.accept(s);
        }, e -> {
            if (e instanceof SessionException.VersionConflict) {
                reload();
            }
            Ui.message(owner, "Not saved", Ui.describe(e));
        });
    }

    void reload() {
        Background.run(() -> services.sessions().get(account.id(), session.id()).orElseThrow(), s -> {
            session = s;
            refresh();
        }, e -> Ui.message(owner, "Cannot reload the session", Ui.describe(e)));
    }

    private void refresh() {
        header.setText(session.name());
        showNote();
        inputs.refresh();
        overrides.refresh();
        pipeline.refresh();
    }

    /**
     * Submit (S8.32): the session's current pipeline. The screen that asked gets the problems Submit
     * found: the errors when it was refused, else the warnings, which are shown and do not block
     * (section 10, Validation). The Runs tab then shows the new run, its warnings with it.
     */
    void submit(SubmitRequest request, Consumer<List<PipelineProblem>> problems) {
        Background.run(() -> {
            List<RunId> ids = services.runs().submit(account.id(), session.id(), request);
            List<PipelineProblem> warnings = ids.stream().flatMap(id -> services.runs().view(account.id(), id).stream())
                    .flatMap(v -> v.warnings().stream()).distinct().toList();
            return new Submitted(ids, warnings);
        }, s -> {
            problems.accept(s.warnings());
            runs.refresh(s.runs().stream().findFirst());
            tabs.getSelectionModel().select(3);
        }, e -> {
            if (e instanceof RunnerException.ValidationFailed v) {
                problems.accept(v.problems());
            } else {
                Ui.message(owner, "The run was not submitted", Ui.describe(e));
            }
        });
    }

    /** One line for what Submit gave: refused, or submitted with or without warnings. */
    static String submitted(List<PipelineProblem> found) {
        return found.stream().filter(PipelineProblem::isError).findFirst().map(p -> "Not submitted: " + p.message())
                .orElse(found.isEmpty() ? "Submitted." : "Submitted, with " + found.size()
                        + (found.size() == 1 ? " warning; the run shows it too." : " warnings; the run shows them too."));
    }

    void onMessages(List<RunMessage> batch) {
        if (runs != null) {
            runs.onMessages(batch);
        }
    }
}
