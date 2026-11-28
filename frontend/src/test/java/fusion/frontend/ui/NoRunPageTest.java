package fusion.frontend.ui;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;

import java.util.List;

import org.junit.jupiter.api.Test;

import fusion.frontend.runner.RunState;

/** Open the run page's answer for a run without a page, by the run's state (prompt log entry 387). */
class NoRunPageTest {
    @Test
    void an_active_run_writes_its_page_when_it_ends() {
        for (RunState state : List.of(RunState.QUEUED, RunState.STARTING, RunState.RUNNING, RunState.STOPPING)) {
            assertEquals(new NoRunPage("No run page yet", "The run page is written when the run ends."),
                    NoRunPage.of(state));
        }
    }

    @Test
    void a_killed_crashed_or_lost_run_ended_before_it_could_write_one() {
        for (RunState state : List.of(RunState.KILLED, RunState.CRASHED, RunState.LOST)) {
            assertEquals("This run ended before it could write its run page. Its files are in its folder.",
                    NoRunPage.of(state).text());
        }
    }

    @Test
    void a_run_that_never_started_has_none() {
        for (RunState state : List.of(RunState.CANCELLED, RunState.NOT_STARTED)) {
            assertEquals("This run never started, so it has no run page.", NoRunPage.of(state).text());
        }
    }

    @Test
    void an_ended_run_without_a_page_was_run_by_an_older_backend() {
        for (RunState state : List.of(RunState.FINISHED, RunState.FAILED, RunState.STOPPED)) {
            assertEquals("This run has no run page: the backend that ran it did not write one. Its files are in its folder.",
                    NoRunPage.of(state).text());
        }
    }

    @Test
    void every_state_has_an_answer() {
        for (RunState state : RunState.values()) {
            assertFalse(NoRunPage.of(state).text().isBlank(), state.name());
        }
    }
}
