package fusion.frontend.ui;

import fusion.frontend.runner.RunState;

/**
 * What Open the run page says when a run has no {@code index.html}. The backend writes the page when a
 * run ends, whatever its status, but a killed or crashed run ends before it can, and a run that never
 * started has none (Phase 3, prompt log entry 387).
 */
record NoRunPage(String title, String text) {
    static NoRunPage of(RunState state) {
        if (state.isActive()) {
            return new NoRunPage("No run page yet", "The run page is written when the run ends.");
        }
        return switch (state) {
            case KILLED, CRASHED, LOST -> new NoRunPage("No run page",
                    "This run ended before it could write its run page. Its files are in its folder.");
            case CANCELLED, NOT_STARTED -> new NoRunPage("No run page",
                    "This run never started, so it has no run page.");
            default -> new NoRunPage("No run page",
                    "This run has no run page: the backend that ran it did not write one. Its files are in its folder.");
        };
    }
}
