package fusion.frontend.runner;

/**
 * A run's states (section 10, Run records and states): queued, starting, running, then finished or
 * failed; stopping and stopped after Stop; cancelled before it started; not_started when the process
 * could not start; killed on Kill; crashed without a final record; lost after a restart.
 */
public enum RunState {
    QUEUED, STARTING, RUNNING, STOPPING, FINISHED, FAILED, STOPPED, KILLED, CRASHED, LOST, CANCELLED, NOT_STARTED;

    /** Queued, starting, running or stopping: the session is busy. */
    public boolean isActive() {
        return this == QUEUED || this == STARTING || this == RUNNING || this == STOPPING;
    }

    /** A process exists or may exist. */
    public boolean isLive() {
        return this == STARTING || this == RUNNING || this == STOPPING;
    }

    /** States from which Resume starts a new run (decision 0019, S1.28). */
    public boolean isResumable() {
        return this == STOPPED || this == KILLED || this == LOST || this == CRASHED || this == FAILED;
    }
}
