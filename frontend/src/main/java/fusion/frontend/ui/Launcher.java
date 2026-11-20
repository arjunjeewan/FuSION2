package fusion.frontend.ui;

import java.io.IOException;
import java.nio.file.Path;
import java.util.Optional;

import javafx.application.Application;

/**
 * The application's entry point, jpackage's main class (section 10, Delivery): it handles
 * {@code --reset-password <username>} and exits; otherwise it claims the data directory, or asks the
 * instance that holds it to bring its window forward and exits (10.C4); then it launches JavaFX.
 * FuSION 1 started from its launch directory and found nothing elsewhere (S8.01).
 */
public final class Launcher {
    private Launcher() {}

    public static void main(String[] args) throws IOException {
        Path home = Path.of(System.getProperty("user.home"));
        Path dataDir = DataDirectory.dataDir(System.getenv(), System.getProperty("os.name"), home);
        Path resultsDir = DataDirectory.resultsDir(System.getenv(), home);
        if (args.length >= 1 && args[0].equals("--reset-password")) {
            if (args.length != 2) {
                System.err.println("usage: fusion-frontend --reset-password <username>");
                System.exit(2);
            }
            System.exit(PasswordReset.run(args[1], dataDir));
        }
        Optional<SingleInstance> instance = SingleInstance.claim(dataDir);
        if (instance.isEmpty()) {
            SingleInstance.askToFocus(dataDir);
            System.out.println("FuSION is already running for " + dataDir + "; its window was asked to come forward.");
            return;
        }
        FusionApplication.prepare(dataDir, resultsDir, instance.get(), args);
        Application.launch(FusionApplication.class, args);
    }
}
