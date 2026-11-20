package fusion.frontend.runner;

import java.util.List;
import java.util.Optional;
import java.util.UUID;
import java.util.function.Consumer;

import fusion.frontend.accounts.AccountId;
import fusion.frontend.sessions.SessionId;
import fusion.frontend.sessions.SessionUse;

/**
 * Runs (A1, A2, S8.28): each one {@code fusion} child process on a batch the frontend writes into a
 * new run directory, followed through its files, stopped through a file, and outliving the frontend
 * (10.C16). Errors: {@link RunnerException.ValidationFailed} (with problems),
 * {@link RunnerException.BackendUnavailable}, {@link RunnerException.NotOwner},
 * {@link RunnerException.NotResumable}, {@link RunnerException.NotKillable}.
 */
public interface RunService extends SessionUse {
    List<RunId> submit(AccountId who, SessionId session, SubmitRequest request);   // one id per swept value

    void stop(AccountId who, RunId run);

    void stopSweep(AccountId who, UUID sweepGroup);

    /** Ends the process and every descendant; offered once the stop grace has passed (10.C19). */
    void kill(AccountId who, RunId run);

    /** True once a stopping run has outlived {@code fusion.runs.stop-grace}, so the screens offer Kill. */
    boolean killOffered(AccountId who, RunId run);

    RunId resume(AccountId who, RunId run);

    Optional<RunView> view(AccountId who, RunId run);

    List<RunRecord> runsOf(AccountId who, SessionId session);

    /** Messages for this account's runs, delivered once per poll tick (10.C5). */
    AutoCloseable subscribe(AccountId who, Consumer<List<RunMessage>> perTick);
}
