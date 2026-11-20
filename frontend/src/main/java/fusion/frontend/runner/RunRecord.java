package fusion.frontend.runner;

import java.nio.file.Path;
import java.time.Instant;
import java.util.Objects;
import java.util.Optional;
import java.util.OptionalInt;
import java.util.OptionalLong;
import java.util.UUID;

import fusion.frontend.accounts.AccountId;
import fusion.frontend.sessions.SessionId;

/** One run's record, {@code <data dir>/runs/<id>.json} (section 10, Run records and states). */
public record RunRecord(RunId id, SessionId session, AccountId owner, PipelineChoice pipeline,
                        String batchSha256, int cores, Optional<Path> runDir, Optional<UUID> sweepGroup,
                        OptionalInt sweepValue, Optional<RunId> resumedFrom, Optional<String> fusionRunId,
                        Instant submitted, Optional<Instant> started, Optional<Instant> ended,
                        OptionalLong pid, Optional<Instant> processStart, RunState state,
                        OptionalInt osExitStatus) {
    public RunRecord {
        Objects.requireNonNull(id, "id");
        Objects.requireNonNull(session, "session");
        Objects.requireNonNull(owner, "owner");
        Objects.requireNonNull(pipeline, "pipeline");
        Objects.requireNonNull(batchSha256, "batchSha256");
        Objects.requireNonNull(runDir, "runDir");
        Objects.requireNonNull(sweepGroup, "sweepGroup");
        Objects.requireNonNull(sweepValue, "sweepValue");
        Objects.requireNonNull(resumedFrom, "resumedFrom");
        Objects.requireNonNull(fusionRunId, "fusionRunId");
        Objects.requireNonNull(submitted, "submitted");
        Objects.requireNonNull(started, "started");
        Objects.requireNonNull(ended, "ended");
        Objects.requireNonNull(pid, "pid");
        Objects.requireNonNull(processStart, "processStart");
        Objects.requireNonNull(state, "state");
        Objects.requireNonNull(osExitStatus, "osExitStatus");
    }

    RunRecord withState(RunState next) {
        return new RunRecord(id, session, owner, pipeline, batchSha256, cores, runDir, sweepGroup, sweepValue,
                resumedFrom, fusionRunId, submitted, started, ended, pid, processStart, next, osExitStatus);
    }

    RunRecord started(Path dir, long processId, Optional<Instant> processStartedAt, Instant at) {
        return new RunRecord(id, session, owner, pipeline, batchSha256, cores, Optional.of(dir), sweepGroup,
                sweepValue, resumedFrom, fusionRunId, submitted, Optional.of(at), ended, OptionalLong.of(processId),
                processStartedAt, RunState.RUNNING, osExitStatus);
    }

    RunRecord withRunDir(Path dir) {
        return new RunRecord(id, session, owner, pipeline, batchSha256, cores, Optional.of(dir), sweepGroup,
                sweepValue, resumedFrom, fusionRunId, submitted, started, ended, pid, processStart, state, osExitStatus);
    }

    RunRecord withFusionRunId(String fusionId) {
        return new RunRecord(id, session, owner, pipeline, batchSha256, cores, runDir, sweepGroup, sweepValue,
                resumedFrom, Optional.of(fusionId), submitted, started, ended, pid, processStart, state, osExitStatus);
    }

    RunRecord ended(RunState finalState, Instant at, OptionalInt exit) {
        return new RunRecord(id, session, owner, pipeline, batchSha256, cores, runDir, sweepGroup, sweepValue,
                resumedFrom, fusionRunId, submitted, started, Optional.of(at), pid, processStart, finalState, exit);
    }
}
