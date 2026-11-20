package fusion.frontend.runner;

import java.time.Instant;

import tools.jackson.databind.JsonNode;

/** What the runner hands the screens, batched once per 250 ms poll tick (10.C5). */
public sealed interface RunMessage {
    RunId run();

    /** One line of {@code events.jsonl}, in section 09's envelope. */
    record Event(RunId run, long seq, JsonNode body) implements RunMessage {}

    record Status(RunId run, RunState state, Instant at) implements RunMessage {}

    /** New text of the run log, from that byte offset. */
    record Log(RunId run, long from, String text) implements RunMessage {}
}
