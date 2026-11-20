package fusion.frontend.runner;

import java.time.Duration;
import java.time.Instant;
import java.time.format.DateTimeParseException;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Deque;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.TreeMap;
import java.util.function.Function;
import java.util.function.IntFunction;

import fusion.frontend.pipeline.Json;
import tools.jackson.databind.JsonNode;

/**
 * Turns a run's events (09.C10, in section 09's envelope {@code v}, {@code seq}, {@code t},
 * {@code type}, with {@code step}, {@code line} and {@code point} inside a step; 1.3) into what the
 * screens show (A2, S8.28). The stage comes from the latest {@code step.started}, named by the
 * catalog's stage label and mapped to its block through the source map; FuSION 1 guessed stages from
 * the command echo ({@code Linux/frontend/src/Gui4Panel.java:197-210}; S1.10). Unknown types and
 * keys are ignored.
 */
final class RunProgress {
    /** A run's view keeps its last 1,000 events in memory (section 10, Following a run). */
    static final int KEEP_EVENTS = 1000;

    private final Function<String, Optional<String>> stageOf;   // command -> catalog stage label
    private final IntFunction<Optional<String>> blockOf;        // batch line -> block id
    private final Deque<JsonNode> recent = new ArrayDeque<>();
    private final List<JsonNode> reports = new ArrayList<>();
    private final Map<String, String> caseStatuses = new TreeMap<>();
    private final Map<String, Integer> pointsByCase = new TreeMap<>();
    private Optional<String> block = Optional.empty();
    private Optional<String> stage = Optional.empty();
    private Optional<String> checking = Optional.empty();
    private Optional<JsonNode> failure = Optional.empty();
    private Optional<String> fusionRunId = Optional.empty();
    private Optional<Instant> started = Optional.empty();
    private Optional<Instant> finished = Optional.empty();
    private Optional<String> finalStatus = Optional.empty();
    private int checks;
    private int unknowns;
    private long lastSeq;

    RunProgress(Function<String, Optional<String>> stageOf, IntFunction<Optional<String>> blockOf) {
        this.stageOf = stageOf;
        this.blockOf = blockOf;
    }

    /** Applies one event line; returns the parsed event, or empty for a line that is not JSON. */
    Optional<JsonNode> apply(String line) {
        JsonNode e;
        try {
            e = Json.parse(line, "events.jsonl");
        } catch (RuntimeException ex) {
            return Optional.empty();
        }
        if (!e.isObject()) {
            return Optional.empty();
        }
        lastSeq = e.path("seq").asLong(lastSeq + 1);
        recent.addLast(e);
        while (recent.size() > KEEP_EVENTS) {
            recent.removeFirst();
        }
        String type = e.path("type").asString("");
        switch (type) {
            case "run.started" -> {
                fusionRunId = Json.text(e, "run");
                started = time(e);
            }
            case "step.started" -> {
                String cmd = e.path("cmd").asString(e.path("step").asString(""));
                block = blockOf.apply(e.path("line").asInt(0));
                stage = stageOf.apply(cmd).or(() -> cmd.isEmpty() ? Optional.empty() : Optional.of(cmd));
                checking = Optional.empty();
            }
            case "step.finished" -> {
                JsonNode report = e.get("report");
                if (report != null && report.isObject()) {
                    reports.add(report);
                }
                JsonNode error = e.get("error");
                if (error != null && error.isObject()) {
                    failure = Optional.of(error);
                }
                checking = Optional.empty();
            }
            case "error" -> failure = Optional.of(e);
            case "solve.check.started" -> checking = Optional.of("case " + e.path("case").asString("?")
                    + ", n=" + e.path("n").asInt() + " e=" + e.path("e").asInt()
                    + time(e).map(t -> ", since " + t).orElse(""));
            case "solve.check" -> {
                checks++;
                String outcome = e.path("outcome").asString("");
                if (outcome.equals("unknown") || outcome.equals("timeout") || outcome.equals("undefined")) {
                    unknowns++;
                }
                checking = Optional.empty();
            }
            case "solve.point" -> pointsByCase.merge(e.path("case").asString("?"), 1, Integer::sum);
            case "solve.case.finished" -> caseStatuses.put(e.path("case").asString("?"), e.path("status").asString("?"));
            case "results.frontier" -> {
                JsonNode points = e.get("points");
                if (points != null && points.isArray()) {
                    pointsByCase.put(e.path("case").asString("?"), points.size());
                }
                Json.text(e, "status").ifPresent(s -> caseStatuses.putIfAbsent(e.path("case").asString("?"), s));
            }
            case "run.finished" -> {
                finalStatus = Json.text(e, "status");
                finished = time(e);
            }
            default -> { }
        }
        return Optional.of(e);
    }

    private static Optional<Instant> time(JsonNode e) {
        try {
            return Json.text(e, "t").map(Instant::parse);
        } catch (DateTimeParseException ex) {
            return Optional.empty();
        }
    }

    Progress progress(Instant now) {
        Instant from = started.orElse(now);
        Instant to = finished.orElse(now);
        Duration elapsed = to.isBefore(from) ? Duration.ZERO : Duration.between(from, to);
        return new Progress(block, stage, checks, pointsByCase, unknowns, checking, elapsed);
    }

    List<JsonNode> reports() {
        return List.copyOf(reports);
    }

    Map<String, String> caseStatuses() {
        return Map.copyOf(caseStatuses);
    }

    Optional<JsonNode> failure() {
        return failure;
    }

    Optional<String> fusionRunId() {
        return fusionRunId;
    }

    Optional<String> finalStatus() {
        return finalStatus;
    }

    List<JsonNode> recent() {
        return List.copyOf(recent);
    }

    void setFailure(JsonNode node) {
        failure = Optional.of(node);
    }
}
