package fusion.frontend.ui;

import java.util.Arrays;
import java.util.function.Consumer;

import javafx.geometry.Pos;
import javafx.scene.Parent;
import javafx.scene.control.Button;
import javafx.scene.control.Hyperlink;
import javafx.scene.control.Label;
import javafx.scene.control.PasswordField;
import javafx.scene.control.TextField;
import javafx.scene.layout.VBox;

import fusion.frontend.accounts.Account;

/**
 * Sign-in, with open sign-up (10.C2): "Create a new account" beside the sign-in, and the sign-up
 * shown first while no account exists. An unknown user and a wrong password get the same message,
 * "Invalid username or password"; FuSION 1 showed "Wrong password!" for both
 * ({@code Linux/frontend/src/LoginPage.java:191-213}; S8.02). There is no email (10.C10).
 */
final class SignInView {
    private final Services services;
    private final Consumer<Account> onSignedIn;
    private final VBox root = new VBox(10);
    private final Label heading = Ui.title("");
    private final TextField username = new TextField();
    private final TextField displayName = new TextField();
    private final PasswordField password = new PasswordField();
    private final PasswordField confirm = new PasswordField();
    private final Label status = Ui.error();
    private final Button primary = new Button();
    private final Hyperlink switcher = new Hyperlink();
    private boolean signUp;

    SignInView(Services services, Consumer<Account> onSignedIn) {
        this.services = services;
        this.onSignedIn = onSignedIn;
        root.setAlignment(Pos.CENTER);
        root.setMaxWidth(360);
        username.setPromptText("username");
        displayName.setPromptText("your name, as the screens show it (optional)");
        password.setPromptText("password, 8 to 72 bytes");
        confirm.setPromptText("the password again");
        primary.setDefaultButton(true);
        primary.setMaxWidth(Double.MAX_VALUE);
        primary.setOnAction(e -> submit());
        switcher.setOnAction(e -> mode(!signUp));
        Label product = Ui.title("FuSION 2.0");
        product.setStyle("-fx-font-size: 26px; -fx-font-weight: bold;");
        root.getChildren().addAll(product, Ui.muted("Signalling paths between genes, from pathways and expression data"),
                heading, username, displayName, password, confirm, primary, status, switcher);
        mode(false);
        Background.run(services.accounts()::isEmpty, empty -> {
            if (empty) {
                mode(true);
            }
        }, e -> status.setText(Ui.describe(e)));
    }

    Parent root() {
        VBox outer = new VBox(root);
        outer.setAlignment(Pos.CENTER);
        return outer;
    }

    private void mode(boolean signUpMode) {
        signUp = signUpMode;
        heading.setText(signUp ? "Create a new account" : "Sign in to your account");
        displayName.setVisible(signUp);
        displayName.setManaged(signUp);
        confirm.setVisible(signUp);
        confirm.setManaged(signUp);
        primary.setText(signUp ? "Create account" : "Sign in");
        switcher.setText(signUp ? "Already have an account? Sign in" : "Create a new account");
        status.setText("");
    }

    private void submit() {
        String user = username.getText();
        char[] pw = password.getText().toCharArray();
        primary.setDisable(true);
        status.setText("");
        if (signUp) {
            char[] again = confirm.getText().toCharArray();
            if (!Arrays.equals(pw, again)) {
                status.setText("Passwords do not match");
                primary.setDisable(false);
                return;
            }
            String name = displayName.getText();
            Background.run(() -> services.accounts().create(user, name, pw), this::done, this::failed);
        } else {
            Background.run(() -> services.accounts().authenticate(user, pw), found -> {
                if (found.isPresent()) {
                    done(found.get());
                } else {
                    password.clear();
                    status.setText("Invalid username or password");
                    primary.setDisable(false);
                }
            }, this::failed);
        }
    }

    private void done(Account account) {
        password.clear();
        confirm.clear();
        onSignedIn.accept(account);
    }

    private void failed(Throwable e) {
        status.setText(Ui.describe(e));
        primary.setDisable(false);
    }
}
