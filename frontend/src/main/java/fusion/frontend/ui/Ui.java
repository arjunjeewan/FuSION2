package fusion.frontend.ui;

import javafx.geometry.Insets;
import javafx.scene.control.Alert;
import javafx.scene.control.ButtonType;
import javafx.scene.control.Label;
import javafx.scene.layout.Region;
import javafx.stage.Window;

/** Small layout and message helpers shared by the screens; Phase 5 designs the real ones. */
final class Ui {
    private Ui() {}

    static Label title(String text) {
        Label l = new Label(text);
        l.setStyle("-fx-font-size: 18px; -fx-font-weight: bold;");
        return l;
    }

    static Label muted(String text) {
        Label l = new Label(text);
        l.setStyle("-fx-text-fill: #555555;");
        l.setWrapText(true);
        return l;
    }

    static Label error() {
        Label l = new Label();
        l.setStyle("-fx-text-fill: #b00020;");
        l.setWrapText(true);
        return l;
    }

    static <T extends Region> T padded(T node) {
        node.setPadding(new Insets(12));
        return node;
    }

    static void message(Window owner, String header, String text) {
        Alert a = new Alert(Alert.AlertType.INFORMATION, text, ButtonType.OK);
        a.initOwner(owner);
        a.setHeaderText(header);
        a.showAndWait();
    }

    static boolean confirm(Window owner, String header, String text) {
        Alert a = new Alert(Alert.AlertType.CONFIRMATION, text, ButtonType.OK, ButtonType.CANCEL);
        a.initOwner(owner);
        a.setHeaderText(header);
        return a.showAndWait().filter(b -> b == ButtonType.OK).isPresent();
    }

    /** A failure's message for the screen: the exception's own, which the services write for people. */
    static String describe(Throwable t) {
        Throwable root = t;
        while (root.getCause() != null && root.getMessage() == null) {
            root = root.getCause();
        }
        return root.getMessage() == null ? root.getClass().getSimpleName() : root.getMessage();
    }
}
