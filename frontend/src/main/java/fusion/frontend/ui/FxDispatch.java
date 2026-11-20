package fusion.frontend.ui;

/** Hands updates to the FX Application Thread, coalesced into one {@code Platform.runLater} (10.C5). */
interface FxDispatch {
    void post(Runnable update);
}
