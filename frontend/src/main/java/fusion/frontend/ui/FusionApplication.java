package fusion.frontend.ui;

import java.io.IOException;
import java.nio.file.Path;
import java.util.LinkedHashMap;
import java.util.Map;

import org.springframework.boot.Banner;
import org.springframework.boot.WebApplicationType;
import org.springframework.boot.builder.SpringApplicationBuilder;
import org.springframework.context.ConfigurableApplicationContext;

import javafx.animation.Animation;
import javafx.animation.KeyFrame;
import javafx.animation.Timeline;
import javafx.application.Application;
import javafx.application.Platform;
import javafx.scene.Scene;
import javafx.stage.Stage;
import javafx.util.Duration;

import fusion.frontend.accounts.Account;

/**
 * The JavaFX application (10.C1): one window, with the Spring context started in {@link #init()},
 * which JavaFX runs on the launcher thread, as a Spring Boot context with
 * {@link WebApplicationType#NONE}: no web server, no port. Closing the main window quits; quitting
 * closes the context, and runs continue (10.C16).
 */
public final class FusionApplication extends Application {
    private static Path dataDir;
    private static Path resultsDir;
    private static SingleInstance instance;
    private static String[] arguments = new String[0];

    private ConfigurableApplicationContext context;
    private Services services;
    private Stage stage;

    static void prepare(Path data, Path results, SingleInstance held, String[] args) {
        dataDir = data;
        resultsDir = results;
        instance = held;
        arguments = args.clone();
    }

    @Override
    public void init() {
        Map<String, Object> defaults = new LinkedHashMap<>();
        defaults.put("fusion.data-dir", dataDir.toString());
        defaults.put("fusion.results-dir", resultsDir.toString());
        // The frontend's own log: logs/frontend.log, rotated at 10 MB, 5 files kept (section 10).
        defaults.put("logging.file.name", dataDir.resolve("logs").resolve("frontend.log").toString());
        defaults.put("logging.logback.rollingpolicy.max-file-size", "10MB");
        defaults.put("logging.logback.rollingpolicy.max-history", "5");
        context = new SpringApplicationBuilder(FrontendConfiguration.class)
                .web(WebApplicationType.NONE)
                .headless(false)
                .bannerMode(Banner.Mode.OFF)
                .logStartupInfo(false)
                .properties(defaults)
                .run(arguments);
        services = Services.from(context, new CoalescingFxDispatch(Platform::runLater),
                page -> getHostServices().showDocument(page.toUri().toString()), new FxFileDialogs());
    }

    @Override
    public void start(Stage primary) {
        stage = primary;
        Platform.setImplicitExit(true);
        stage.setTitle("FuSION 2.0");
        stage.setMinWidth(900);
        stage.setMinHeight(600);
        showSignIn();
        stage.show();
        // A second launch asks for the window through frontend.focus, seen within one tick (10.C4).
        Timeline focus = new Timeline(new KeyFrame(Duration.millis(250), e -> {
            if (instance != null && instance.focusRequested()) {
                stage.setIconified(false);
                stage.toFront();
                stage.requestFocus();
            }
        }));
        focus.setCycleCount(Animation.INDEFINITE);
        focus.play();
    }

    private void showSignIn() {
        SignInView view = new SignInView(services, this::showMain);
        stage.setScene(new Scene(view.root(), 1100, 760));
    }

    private void showMain(Account account) {
        MainView view = new MainView(services, account, stage, this::showSignIn, Platform::exit);
        stage.setScene(new Scene(view.root(), 1280, 820));
    }

    @Override
    public void stop() throws IOException {
        if (context != null) {
            context.close();
        }
        if (instance != null) {
            instance.close();
        }
    }
}
