package fusion.frontend.ui;

import java.util.function.Consumer;
import java.util.function.Supplier;

import javafx.application.Platform;

/**
 * The threading rule (section 10, Threads): every service call, file read and process wait runs on a
 * virtual thread, and its result comes back on the FX Application Thread, which only draws and
 * handles input.
 */
final class Background {
    private Background() {}

    static <T> void run(Supplier<T> work, Consumer<T> onResult, Consumer<Throwable> onError) {
        Thread.ofVirtual().name("fusion-ui-work").start(() -> {
            try {
                T result = work.get();
                Platform.runLater(() -> onResult.accept(result));
            } catch (RuntimeException | Error e) {
                Platform.runLater(() -> onError.accept(e));
            }
        });
    }
}
