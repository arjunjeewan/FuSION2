package fusion.frontend.runner;

import java.io.IOException;
import java.nio.file.DirectoryStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Instant;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import java.util.OptionalInt;
import java.util.OptionalLong;
import java.util.UUID;

import fusion.frontend.accounts.AccountId;
import fusion.frontend.pipeline.AtomicFiles;
import fusion.frontend.pipeline.Json;
import fusion.frontend.pipeline.PipelineProblem;
import fusion.frontend.pipeline.Severity;
import fusion.frontend.pipeline.SideFile;
import fusion.frontend.pipeline.Wire;
import fusion.frontend.sessions.SessionId;
import tools.jackson.databind.JsonNode;
import tools.jackson.databind.node.ArrayNode;
import tools.jackson.databind.node.ObjectNode;

/**
 * Run records, {@code <data dir>/runs/<id>.json}, one file per run, written by atomic replace
 * (10.C8). Besides section 10's fields a record keeps, until its run starts, the batch text and side
 * files, so a queued run survives a restart of the frontend; the label of its run directory, how
 * it resumes an earlier run, when Stop and Kill were asked for, and the warnings Submit's checks gave
 * it, which its view keeps showing.
 */
final class RunStore {
    static final int FORMAT = 1;
    private static final System.Logger LOG = System.getLogger(RunStore.class.getName());

    /** What a queued run starts with. */
    record Pending(String batch, List<SideFile> sideFiles) {}

    /** How a run continues an earlier one (01.C6, 09.C5): the same batch, or the session's corrected one. */
    enum ResumeMode { NONE, CONTINUE, RESTART }

    /** A record with the frontend's own bookkeeping. */
    record Stored(RunRecord record, String label, Optional<Path> resultsRoot, Optional<Pending> pending,
                  ResumeMode resume, Optional<Path> resumeDir, Optional<Instant> stopRequested,
                  boolean killRequested, Optional<String> note, List<PipelineProblem> warnings) {
        Stored {
            warnings = List.copyOf(warnings);
        }

        Stored with(RunRecord next) {
            return new Stored(next, label, resultsRoot, pending, resume, resumeDir, stopRequested, killRequested, note,
                    warnings);
        }

        Stored started(RunRecord next) {
            return new Stored(next, label, resultsRoot, Optional.empty(), resume, resumeDir, stopRequested,
                    killRequested, note, warnings);
        }

        Stored stopRequestedAt(Instant at) {
            return new Stored(record, label, resultsRoot, pending, resume, resumeDir, Optional.of(at), killRequested, note,
                    warnings);
        }

        Stored withKillRequested() {
            return new Stored(record, label, resultsRoot, pending, resume, resumeDir, stopRequested, true, note, warnings);
        }

        Stored withNote(String text) {
            return new Stored(record, label, resultsRoot, pending, resume, resumeDir, stopRequested, killRequested,
                    Optional.of(text), warnings);
        }
    }

    private final Path dir;

    RunStore(Path dataDir) {
        this.dir = dataDir.resolve("runs");
    }

    void save(Stored s) {
        RunRecord r = s.record();
        ObjectNode o = Json.object();
        o.put("format", FORMAT);
        o.put("id", r.id().toString());
        o.put("session", r.session().toString());
        o.put("owner", r.owner().toString());
        o.put("pipeline", Wire.name(r.pipeline()));
        o.put("batch_sha256", r.batchSha256());
        o.put("cores", r.cores());
        putOpt(o, "run_dir", r.runDir().map(Path::toString));
        putOpt(o, "sweep_group", r.sweepGroup().map(UUID::toString));
        if (r.sweepValue().isPresent()) {
            o.put("sweep_value", r.sweepValue().getAsInt());
        } else {
            o.putNull("sweep_value");
        }
        putOpt(o, "resumed_from", r.resumedFrom().map(RunId::toString));
        putOpt(o, "fusion_run_id", r.fusionRunId());
        o.put("submitted", r.submitted().toString());
        putOpt(o, "started", r.started().map(Instant::toString));
        putOpt(o, "ended", r.ended().map(Instant::toString));
        if (r.pid().isPresent()) {
            o.put("pid", r.pid().getAsLong());
        } else {
            o.putNull("pid");
        }
        putOpt(o, "process_start", r.processStart().map(Instant::toString));
        o.put("state", Wire.name(r.state()));
        if (r.osExitStatus().isPresent()) {
            o.put("os_exit_status", r.osExitStatus().getAsInt());
        } else {
            o.putNull("os_exit_status");
        }
        o.put("label", s.label());
        putOpt(o, "results_root", s.resultsRoot().map(Path::toString));
        o.put("resume", Wire.name(s.resume()));
        putOpt(o, "resume_dir", s.resumeDir().map(Path::toString));
        putOpt(o, "stop_requested", s.stopRequested().map(Instant::toString));
        o.put("kill_requested", s.killRequested());
        putOpt(o, "note", s.note());
        ArrayNode warnings = o.putArray("warnings");
        for (PipelineProblem p : s.warnings()) {
            ObjectNode w = warnings.addObject();
            w.put("severity", Wire.name(p.severity()));
            putOpt(w, "block", p.block());
            putOpt(w, "argument", p.argument());
            putOpt(w, "input", p.input());
            w.put("code", p.code());
            w.put("message", p.message());
        }
        if (s.pending().isPresent()) {
            o.put("batch", s.pending().get().batch());
            ArrayNode files = o.putArray("side_files");
            for (SideFile f : s.pending().get().sideFiles()) {
                ObjectNode fo = files.addObject();
                fo.put("path", f.relativePath());
                ArrayNode lines = fo.putArray("lines");
                f.lines().forEach(lines::add);
            }
        }
        try {
            AtomicFiles.write(dir.resolve(r.id() + ".json"), Json.pretty(o), true);
        } catch (IOException e) {
            throw new RunnerException("run.store", "cannot write the record of run " + r.id() + ": " + e.getMessage());
        }
    }

    List<Stored> loadAll() {
        List<Stored> out = new ArrayList<>();
        if (!Files.isDirectory(dir)) {
            return out;
        }
        try (DirectoryStream<Path> files = Files.newDirectoryStream(dir, "*.json")) {
            for (Path f : files) {
                try {
                    out.add(read(Json.parse(Files.readString(f), f.toString())));
                } catch (IOException | RuntimeException e) {
                    LOG.log(System.Logger.Level.WARNING, "skipped run record " + f + ": " + e.getMessage());
                }
            }
        } catch (IOException e) {
            throw new RunnerException("run.store", "cannot read " + dir + ": " + e.getMessage());
        }
        return out;
    }

    static Stored read(JsonNode o) {
        String what = "a run record";
        RunRecord r = new RunRecord(
                new RunId(UUID.fromString(Json.requiredText(o, "id", what))),
                new SessionId(UUID.fromString(Json.requiredText(o, "session", what))),
                new AccountId(UUID.fromString(Json.requiredText(o, "owner", what))),
                Wire.parse(PipelineChoice.class, Json.text(o, "pipeline").orElse("default"), "pipeline"),
                Json.text(o, "batch_sha256").orElse(""),
                o.path("cores").asInt(1),
                Json.text(o, "run_dir").map(Path::of),
                Json.text(o, "sweep_group").map(UUID::fromString),
                o.path("sweep_value").isIntegralNumber() ? OptionalInt.of(o.get("sweep_value").asInt()) : OptionalInt.empty(),
                Json.text(o, "resumed_from").map(t -> new RunId(UUID.fromString(t))),
                Json.text(o, "fusion_run_id"),
                Instant.parse(Json.requiredText(o, "submitted", what)),
                Json.text(o, "started").map(Instant::parse),
                Json.text(o, "ended").map(Instant::parse),
                o.path("pid").isIntegralNumber() ? OptionalLong.of(o.get("pid").asLong()) : OptionalLong.empty(),
                Json.text(o, "process_start").map(Instant::parse),
                Wire.parse(RunState.class, Json.requiredText(o, "state", what), "state"),
                o.path("os_exit_status").isIntegralNumber() ? OptionalInt.of(o.get("os_exit_status").asInt())
                        : OptionalInt.empty());
        Optional<Pending> pending = Optional.empty();
        if (o.has("batch") && o.get("batch").isString()) {
            List<SideFile> files = new ArrayList<>();
            for (JsonNode f : o.path("side_files")) {
                List<String> lines = new ArrayList<>();
                f.path("lines").forEach(l -> lines.add(l.asString()));
                files.add(new SideFile(Json.requiredText(f, "path", what), lines));
            }
            pending = Optional.of(new Pending(o.get("batch").stringValue(), files));
        }
        List<PipelineProblem> warnings = new ArrayList<>();
        for (JsonNode w : o.path("warnings")) {
            warnings.add(new PipelineProblem(Wire.parse(Severity.class, Json.text(w, "severity").orElse("warning"),
                    "severity"), Json.text(w, "block"), Json.text(w, "argument"), Json.text(w, "input"),
                    Json.requiredText(w, "code", "a run's warning"), Json.text(w, "message").orElse("")));
        }
        return new Stored(r, Json.text(o, "label").orElse(""), Json.text(o, "results_root").map(Path::of), pending,
                Wire.parse(ResumeMode.class, Json.text(o, "resume").orElse("none"), "resume"),
                Json.text(o, "resume_dir").map(Path::of), Json.text(o, "stop_requested").map(Instant::parse),
                Json.flag(o, "kill_requested", false), Json.text(o, "note"), warnings);
    }

    private static void putOpt(ObjectNode o, String key, Optional<String> v) {
        if (v.isPresent()) {
            o.put(key, v.get());
        } else {
            o.putNull(key);
        }
    }
}
