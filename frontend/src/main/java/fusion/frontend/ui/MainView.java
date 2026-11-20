package fusion.frontend.ui;

import java.time.ZoneId;
import java.time.format.DateTimeFormatter;
import java.util.List;
import java.util.Locale;
import java.util.Optional;

import javafx.geometry.Insets;
import javafx.scene.Parent;
import javafx.scene.control.Button;
import javafx.scene.control.Label;
import javafx.scene.control.ListCell;
import javafx.scene.control.ListView;
import javafx.scene.control.Menu;
import javafx.scene.control.MenuBar;
import javafx.scene.control.MenuItem;
import javafx.scene.control.TextInputDialog;
import javafx.scene.layout.BorderPane;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Priority;
import javafx.scene.layout.VBox;
import javafx.stage.Stage;

import fusion.frontend.accounts.Account;
import fusion.frontend.runner.RunMessage;
import fusion.frontend.sessions.SessionId;
import fusion.frontend.sessions.SessionSummary;

/**
 * The main window (10.C4): the user's sessions on the left, newest change first (S8.04), the open
 * session in the middle, and the backend's state below. The File menu holds New session, Sign out
 * and Quit; quitting leaves runs going, and the status line says how many (10.C16). FuSION 1 listed
 * sessions by position and opened one even on Cancel ({@code Linux/frontend/src/SessionFrame.java:57-268}).
 */
final class MainView {
    private static final DateTimeFormatter WHEN = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm", Locale.ROOT)
            .withZone(ZoneId.systemDefault());

    private final Services services;
    private final Account account;
    private final Stage stage;
    private final BorderPane root = new BorderPane();
    private final ListView<SessionSummary> list = new ListView<>();
    private final Label backend = Ui.muted("Checking the backend...");
    private final Label status = Ui.muted("");
    private SessionView open;
    private AutoCloseable subscription;

    MainView(Services services, Account account, Stage stage, Runnable onSignOut, Runnable onQuit) {
        this.services = services;
        this.account = account;
        this.stage = stage;

        MenuItem newSession = new MenuItem("New session...");
        newSession.setOnAction(e -> newSession());
        MenuItem signOut = new MenuItem("Sign out");
        signOut.setOnAction(e -> {
            close();
            onSignOut.run();
        });
        MenuItem quit = new MenuItem("Quit");
        quit.setOnAction(e -> {
            close();
            onQuit.run();
        });
        MenuBar menu = new MenuBar(new Menu("File", null, newSession, signOut, quit));
        root.setTop(new VBox(menu));

        list.setCellFactory(v -> new ListCell<>() {
            @Override
            protected void updateItem(SessionSummary s, boolean empty) {
                super.updateItem(s, empty);
                setText(empty || s == null ? null : s.name() + "\n  changed " + WHEN.format(s.updated()));
            }
        });
        list.getSelectionModel().selectedItemProperty().addListener((obs, was, now) -> {
            if (now != null && (open == null || !open.id().equals(now.id()))) {
                openSession(now.id());
            }
        });
        Button add = new Button("New");
        add.setOnAction(e -> newSession());
        Button delete = new Button("Delete");
        delete.setOnAction(e -> deleteSelected());
        HBox buttons = new HBox(8, add, delete);
        VBox left = new VBox(8, Ui.title("Sessions"), Ui.muted("Signed in as " + account.displayName()), list, buttons);
        VBox.setVgrow(list, Priority.ALWAYS);
        left.setPadding(new Insets(12));
        left.setPrefWidth(280);
        root.setLeft(left);
        root.setCenter(Ui.padded(new VBox(Ui.muted("Open a session, or make a new one with File > New session."))));
        HBox bottom = new HBox(24, backend, status);
        bottom.setPadding(new Insets(6, 12, 6, 12));
        root.setBottom(bottom);

        // Called on the runner's thread, once per tick; the count is taken there, off the FX thread.
        subscription = services.runs().subscribe(account.id(), batch -> {
            long active = services.runs().activeRuns(account.id());
            services.fx().post(() -> onMessages(batch, active));
        });
        refreshList(Optional.empty());
        checkBackend();
    }

    Parent root() {
        return root;
    }

    /** fusion --version --json and --commands at start (section 10, Backend checks at start). */
    private void checkBackend() {
        Background.run(() -> {
            services.runs().checkBackend();
            return services.backend().info();
        }, info -> backend.setText("Backend: fusion " + info.fusionVersion() + " ready"
                + (info.failedProbes().isEmpty() ? "" : "; not found: " + String.join(", ", info.failedProbes()))),
                e -> backend.setText("Submit is disabled: " + Ui.describe(e)));
    }

    private void onMessages(List<RunMessage> batch, long active) {
        if (open != null) {
            open.onMessages(batch);
        }
        status.setText(active == 0 ? "" : active + (active == 1 ? " run is" : " runs are")
                + " active; runs continue if FuSION quits.");
    }

    private void refreshList(Optional<SessionId> select) {
        Background.run(() -> services.sessions().list(account.id()), sessions -> {
            list.getItems().setAll(sessions);
            select.flatMap(id -> sessions.stream().filter(s -> s.id().equals(id)).findFirst())
                    .ifPresent(s -> list.getSelectionModel().select(s));
        }, e -> status.setText(Ui.describe(e)));
    }

    private void openSession(SessionId id) {
        Background.run(() -> services.sessions().get(account.id(), id).orElseThrow(), session -> {
            open = new SessionView(services, account, session, stage, () -> refreshList(Optional.of(id)));
            root.setCenter(open.root());
        }, e -> status.setText(Ui.describe(e)));
    }

    /** New Session: a blank name or Cancel creates nothing (S8.04). */
    private void newSession() {
        TextInputDialog d = new TextInputDialog();
        d.initOwner(stage);
        d.setHeaderText("New session");
        d.setContentText("Name:");
        d.showAndWait().ifPresent(name -> Background.run(() -> services.sessions().create(account.id(), name),
                s -> refreshList(Optional.of(s.id())), e -> Ui.message(stage, "No session was made", Ui.describe(e))));
    }

    private void deleteSelected() {
        SessionSummary s = list.getSelectionModel().getSelectedItem();
        if (s == null || !Ui.confirm(stage, "Delete " + s.name() + "?",
                "The session is deleted. Its run folders are never deleted and stay in the results folder.")) {
            return;
        }
        Background.run(() -> {
            services.sessions().delete(account.id(), s.id());
            return s.id();
        }, id -> {
            if (open != null && open.id().equals(id)) {
                open = null;
                root.setCenter(Ui.padded(new VBox(Ui.muted("The session was deleted."))));
            }
            refreshList(Optional.empty());
        }, e -> Ui.message(stage, "Not deleted", Ui.describe(e)));
    }

    private void close() {
        try {
            if (subscription != null) {
                subscription.close();
            }
        } catch (Exception e) {
            // nothing to undo
        }
        subscription = null;
    }
}
