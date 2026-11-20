package fusion.frontend.ui;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Executor;

/**
 * The one hand-over to JavaFX (10.C5): updates posted while one hand-over is pending join it, so
 * the FX thread wakes at most once per poll tick, whether 1 run is followed or 18. The executor is
 * {@code Platform::runLater} in the application and a fake in tests.
 */
final class CoalescingFxDispatch implements FxDispatch {
    private final Executor fx;
    private final List<Runnable> pending = new ArrayList<>();
    private boolean scheduled;

    CoalescingFxDispatch(Executor fx) {
        this.fx = fx;
    }

    @Override
    public void post(Runnable update) {
        boolean schedule;
        synchronized (this) {
            pending.add(update);
            schedule = !scheduled;
            scheduled = true;
        }
        if (schedule) {
            fx.execute(this::drain);
        }
    }

    private void drain() {
        List<Runnable> batch;
        synchronized (this) {
            batch = new ArrayList<>(pending);
            pending.clear();
            scheduled = false;
        }
        batch.forEach(Runnable::run);
    }
}
