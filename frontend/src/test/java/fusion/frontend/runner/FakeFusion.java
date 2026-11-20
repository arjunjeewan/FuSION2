package fusion.frontend.runner;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.nio.file.StandardOpenOption;
import java.time.Instant;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * A stand-in for {@code fusion} in the runner's process tests, until the backend runs batches
 * (section 12, Frontend contracts: "fake-fusion"). It answers {@code --version --json},
 * {@code --commands}, {@code --check --json} and {@code --query} from files its environment names
 * ({@code FAKE_FUSION_LABELS}, {@code FAKE_FUSION_CUDF} and {@code FAKE_FUSION_SUGGEST}, by the
 * line's command; an answer with {@code "ok": false} is a refusal and exits 1, as fusion's does, and
 * {@code FAKE_FUSION_QUERIES} collects the lines asked),
 * and in a run it claims the run directory with {@code run.lock}, holding an {@code fcntl} lock on
 * one byte at 2^31 as {@code fusion} does (09.C9), writes {@code run.json} and {@code events.jsonl}
 * in section 09's formats and follows a JSON-lines script ({@code FAKE_FUSION_SCRIPT}), one
 * directive per line:
 *
 * <pre>
 * {"after_ms": 0, "event": {"type": "step.started", "cmd": "pathz3"}}   the line is found in run.batch
 * {"after_ms": 0, "log": "a line of the run log"}
 * {"after_ms": 0, "spawn_child": true}                                  writes child.pid
 * {"after_ms": 0, "on_stop": {"action": "honour", "within_ms": 100}}    or "ignore", "forced_after_ms"
 * {"after_ms": 0, "wait_for": "go"}                                     a file in the run directory
 * {"after_ms": 0, "write": {"path": "plot_pareto/pareto.png", "text": "..."}}
 * {"after_ms": 0, "crash": true}                                        SIGKILL to itself
 * {"after_ms": 0, "exit": 1}
 * </pre>
 *
 * It writes and reads its JSON by hand, without Jackson, so that each start takes about 60 ms.
 */
public final class FakeFusion {
    private FakeFusion() {}

    private static final Map<String, String> ENV = System.getenv();
    private static Path runDir;
    private static String runId;
    private static long seq;
    private static String started;
    private static String labels = "";
    private static String commandLine = "";
    private static String stop = "null";
    private static volatile String stopAction = "none";
    private static volatile long stopWithin;
    private static volatile long forcedAfter = -1;

    public static void main(String[] args) throws Exception {
        List<String> a = List.of(args);
        record(a);
        String sleep = ENV.get("FAKE_FUSION_SLEEP_MS");
        if (sleep != null && !a.contains("--run-dir")) {
            Thread.sleep(Long.parseLong(sleep));
        }
        String version = ENV.getOrDefault("FAKE_FUSION_VERSION", "0.1.0");
        if (a.contains("--version")) {
            if (a.contains("--json") && ENV.containsKey("FAKE_FUSION_NO_JSON")) {
                System.err.println("error: unknown option --json");
                System.exit(2);
            } else if (a.contains("--json")) {
                System.out.println("{\"versions\":{\"fusion\":{\"version\":" + q(version) + "}},\"install\":{\"home\":"
                        + q(ENV.getOrDefault("FAKE_FUSION_HOME", "")) + "}}");
            } else {
                System.out.println("FuSION " + version + " (fake) at /nowhere");
            }
            return;
        }
        if (a.contains("--commands")) {
            System.out.print(Files.readString(Path.of(ENV.get("FAKE_FUSION_CATALOG"))));
            return;
        }
        if (a.contains("--query")) {
            String line = new String(System.in.readAllBytes(), StandardCharsets.UTF_8).strip();
            String queries = ENV.get("FAKE_FUSION_QUERIES");
            if (queries != null) {
                Files.writeString(Path.of(queries), line + "\n", StandardOpenOption.CREATE, StandardOpenOption.APPEND);
            }
            String env = switch (line.split(" ", 2)[0]) {
                case "labels" -> "FAKE_FUSION_LABELS";
                case "cudf" -> "FAKE_FUSION_CUDF";
                case "suggest_thresholds" -> "FAKE_FUSION_SUGGEST";
                default -> "FAKE_FUSION_NONE";
            };
            Path answer = Path.of(ENV.getOrDefault(env, "/nonexistent"));
            if (!Files.exists(answer)) {
                System.err.println("error: cannot answer " + line);
                System.exit(1);
            }
            String text = Files.readString(answer);
            System.out.print(text);
            System.exit(Pattern.compile("\"ok\"\\s*:\\s*false").matcher(text).find() ? 1 : 0);
            return;
        }
        if (a.contains("--check")) {
            String answer = ENV.containsKey("FAKE_FUSION_CHECK") ? Files.readString(Path.of(ENV.get("FAKE_FUSION_CHECK")))
                    : "{\"ok\": true, \"diagnostics\": [], \"features\": []}";
            System.out.print(answer);
            System.exit(Pattern.compile("\"ok\"\\s*:\\s*false").matcher(answer).find() ? 1 : 0);
            return;
        }
        if (a.contains("--run-dir")) {
            run(a);
            return;
        }
        System.err.println("usage error");
        System.exit(2);
    }

    /** Appends the command line to {@code FAKE_FUSION_RECORD}, for the tests to read. */
    private static void record(List<String> args) throws IOException {
        String file = ENV.get("FAKE_FUSION_RECORD");
        if (file != null) {
            Files.writeString(Path.of(file), String.join(" ", args) + "\n", StandardOpenOption.CREATE,
                    StandardOpenOption.APPEND);
        }
    }

    // ---- a run ----------------------------------------------------------------------------------

    private static void run(List<String> args) throws Exception {
        runDir = Path.of(args.get(args.indexOf("--run-dir") + 1));
        long pid = ProcessHandle.current().pid();
        runId = "r-" + Instant.now().toString().replaceAll("[-:]", "").replaceAll("\\..*", "") + "Z-fa4ef05e";
        FileChannel lock;
        try {
            lock = FileChannel.open(runDir.resolve("run.lock"), StandardOpenOption.CREATE_NEW, StandardOpenOption.WRITE,
                    StandardOpenOption.READ);
        } catch (java.nio.file.FileAlreadyExistsException e) {
            System.err.println("error: " + runDir + " has been used [run.dir_used]");
            System.exit(1);
            return;
        }
        lock.write(ByteBuffer.wrap((pid + " " + runId + "\n").getBytes(StandardCharsets.UTF_8)));
        lock.lock(1L << 31, 1, false);   // held until the process ends, released by the OS

        List<String> tags = new ArrayList<>();
        for (int i = 0; i < args.size() - 1; i++) {
            if (args.get(i).equals("--tag")) {
                String[] kv = args.get(i + 1).split("=", 2);
                tags.add(q(kv[0]) + ":" + q(kv.length > 1 ? kv[1] : ""));
            }
        }
        labels = String.join(",", tags);
        commandLine = String.join(",", args.stream().map(FakeFusion::q).toList());
        started = Instant.now().toString();
        writeRunJson("running", "null", "null", "null");
        event("{\"type\":\"run.started\",\"run\":" + q(runId) + ",\"name\":" + q(runDir.getFileName().toString())
                + ",\"dir\":" + q(runDir.toString()) + ",\"pid\":" + pid + "}");
        log("info  FuSION fake, run " + runId);
        log("info  Run directory: " + runDir);

        Thread watcher = new Thread(FakeFusion::watchStop, "stop-watcher");
        watcher.setDaemon(true);
        watcher.start();

        String script = ENV.get("FAKE_FUSION_SCRIPT");
        for (String step : script == null ? List.<String>of() : Files.readAllLines(Path.of(script))) {
            if (step.isBlank()) {
                continue;
            }
            Thread.sleep(number(step, "after_ms", 0));
            if (step.contains("\"event\"")) {
                String body = object(step, "event");
                Matcher cmd = Pattern.compile("\"cmd\"\\s*:\\s*\"([^\"]*)\"").matcher(body);
                if (cmd.find() && !body.contains("\"line\"")) {
                    body = "{\"line\":" + lineOf(cmd.group(1)) + "," + body.substring(1);
                }
                event(body);
            } else if (step.contains("\"log\"")) {
                log(text(step, "log"));
            } else if (step.contains("\"spawn_child\"")) {
                Process child = new ProcessBuilder("sleep", "600").redirectOutput(ProcessBuilder.Redirect.DISCARD).start();
                Files.writeString(runDir.resolve("child.pid"), Long.toString(child.pid()));
            } else if (step.contains("\"on_stop\"")) {
                String p = object(step, "on_stop");
                stopWithin = number(p, "within_ms", 0);
                forcedAfter = number(p, "forced_after_ms", -1);
                stopAction = text(p, "action");
            } else if (step.contains("\"wait_for\"")) {
                Path f = runDir.resolve(text(step, "wait_for"));
                while (!Files.exists(f)) {
                    Thread.sleep(20);
                }
            } else if (step.contains("\"write\"")) {
                String w = object(step, "write");
                Path f = runDir.resolve(text(w, "path"));
                Files.createDirectories(f.getParent());
                Files.writeString(f, text(w, "text"));
            } else if (step.contains("\"crash\"")) {
                new ProcessBuilder("kill", "-9", Long.toString(ProcessHandle.current().pid())).start().waitFor();
                Thread.sleep(10_000);
            } else if (step.contains("\"exit\"")) {
                finish((int) number(step, "exit", 0), false);
            }
        }
        finish(0, false);
    }

    /** The batch line of a command's first line in run.batch, as section 01 numbers lines. */
    private static int lineOf(String cmd) {
        try {
            List<String> lines = Files.readAllLines(runDir.resolve("run.batch"));
            for (int i = 0; i < lines.size(); i++) {
                if (lines.get(i).equals(cmd) || lines.get(i).startsWith(cmd + " ")) {
                    return i + 1;
                }
            }
        } catch (IOException e) {
            // a resumed run has no run.batch
        }
        return 0;
    }

    private static void watchStop() {
        Path file = runDir.resolve("stop");
        try {
            while (!Files.exists(file)) {
                Thread.sleep(50);
            }
            event("{\"type\":\"stop.requested\",\"source\":\"file\"}");
            synchronized (FakeFusion.class) {
                stop = "{\"source\":\"file\",\"note\":" + q(Files.readString(file).strip()) + ",\"forced\":false}";
            }
            if (stopAction.equals("ignore")) {
                if (forcedAfter >= 0) {
                    Thread.sleep(forcedAfter);
                    finish(3, true);
                }
                return;
            }
            Thread.sleep(stopWithin);
            finish(3, false);
        } catch (InterruptedException | IOException e) {
            // the process is ending
        }
    }

    private static synchronized void finish(int exit, boolean forced) throws IOException {
        String status = switch (exit) {
            case 0 -> "ok";
            case 3, 130 -> "stopped";
            default -> "failed";
        };
        if (forced) {
            stop = stop.replace("\"forced\":false", "\"forced\":true");
        }
        String error = exit == 1 ? "{\"code\":\"fake.failed\",\"message\":\"the fake was told to fail\"}" : "null";
        writeRunJson(status, Integer.toString(exit), q(Instant.now().toString()), error);
        event("{\"type\":\"run.finished\",\"status\":" + q(status) + ",\"exit\":" + exit + ",\"forced\":" + forced + "}");
        log("info  Finished: " + status + ", exit " + exit);
        Runtime.getRuntime().halt(exit);
    }

    private static synchronized void writeRunJson(String status, String exit, String finished, String error)
            throws IOException {
        String json = "{\n \"format\": \"fusion.run\",\n \"format_version\": 1,\n \"run\": " + q(runId)
                + ",\n \"name\": " + q(runDir.getFileName().toString()) + ",\n \"status\": " + q(status)
                + ",\n \"exit\": " + exit + ",\n \"pid\": " + ProcessHandle.current().pid()
                + ",\n \"labels\": {" + labels + "},\n \"command_line\": [" + commandLine + "],\n \"started\": "
                + q(started) + ",\n \"finished\": " + finished
                + ",\n \"run_settings\": [{\"key\": \"run.log\", \"value\": " + q(runDir.getFileName() + ".log")
                + ", \"source\": \"default\"}],\n \"inputs\": [],\n \"steps\": [],\n \"reports\": [],\n \"error\": "
                + error + ",\n \"stop\": " + stop + "\n}\n";
        Path tmp = runDir.resolve("run.json.tmp");
        Files.writeString(tmp, json);
        Files.move(tmp, runDir.resolve("run.json"), StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING);
    }

    /** Appends one event, the envelope v, seq and t in front of the body's members (1.3). */
    private static synchronized void event(String body) throws IOException {
        String rest = body.strip().substring(1).strip();
        String line = "{\"v\":1,\"seq\":" + (++seq) + ",\"t\":" + q(Instant.now().toString())
                + (rest.equals("}") ? "}" : "," + rest);
        Files.writeString(runDir.resolve("events.jsonl"), line + "\n", StandardOpenOption.CREATE, StandardOpenOption.APPEND);
    }

    private static synchronized void log(String text) throws IOException {
        Files.writeString(runDir.resolve(runDir.getFileName() + ".log"), Instant.now() + " " + text + "\n",
                StandardOpenOption.CREATE, StandardOpenOption.APPEND);
    }

    // ---- the little JSON this fake needs --------------------------------------------------------

    static String q(String s) {
        StringBuilder sb = new StringBuilder("\"");
        for (char c : s.toCharArray()) {
            switch (c) {
                case '"' -> sb.append("\\\"");
                case '\\' -> sb.append("\\\\");
                case '\n' -> sb.append("\\n");
                case '\t' -> sb.append("\\t");
                case '\r' -> sb.append("\\r");
                default -> {
                    if (c < 0x20) {
                        sb.append(String.format("\\u%04x", (int) c));
                    } else {
                        sb.append(c);
                    }
                }
            }
        }
        return sb.append('"').toString();
    }

    private static long number(String json, String key, long fallback) {
        Matcher m = Pattern.compile("\"" + key + "\"\\s*:\\s*(-?\\d+)").matcher(json);
        return m.find() ? Long.parseLong(m.group(1)) : fallback;
    }

    private static String text(String json, String key) {
        Matcher m = Pattern.compile("\"" + key + "\"\\s*:\\s*\"((?:[^\"\\\\]|\\\\.)*)\"").matcher(json);
        if (!m.find()) {
            return "";
        }
        return m.group(1).replace("\\n", "\n").replace("\\\"", "\"").replace("\\\\", "\\");
    }

    /** The text of a member that is an object, matched by braces. */
    private static String object(String json, String key) {
        Matcher m = Pattern.compile("\"" + key + "\"\\s*:\\s*\\{").matcher(json);
        if (!m.find()) {
            return "{}";
        }
        int start = m.end() - 1;
        int depth = 0;
        boolean inString = false;
        for (int i = start; i < json.length(); i++) {
            char c = json.charAt(i);
            if (inString) {
                if (c == '\\') {
                    i++;
                } else if (c == '"') {
                    inString = false;
                }
            } else if (c == '"') {
                inString = true;
            } else if (c == '{') {
                depth++;
            } else if (c == '}' && --depth == 0) {
                return json.substring(start, i + 1);
            }
        }
        return "{}";
    }

    /** Writes the wrapper script that starts this class in its own JVM, with an environment. */
    public static Path launcher(Path dir, Map<String, String> env) throws IOException {
        String java = Path.of(System.getProperty("java.home"), "bin", "java").toString();
        List<String> lines = new ArrayList<>();
        lines.add("#!/bin/sh");
        env.forEach((k, v) -> lines.add("export " + k + "='" + v.replace("'", "'\\''") + "'"));
        // The test classes alone: the fake needs nothing else, and a short class path starts faster.
        String cp = Path.of(FakeFusion.class.getProtectionDomain().getCodeSource().getLocation().getPath()).toString();
        lines.add("exec '" + java + "' -XX:+UseSerialGC -XX:TieredStopAtLevel=1 -Xshare:auto -cp '" + cp + "' "
                + FakeFusion.class.getName() + " \"$@\"");
        Path script = dir.resolve("fake-fusion");
        Files.write(script, lines);
        script.toFile().setExecutable(true);
        return script;
    }
}
