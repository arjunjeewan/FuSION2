package fusion.frontend.runner;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Duration;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Optional;
import java.util.OptionalInt;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import fusion.frontend.pipeline.Catalog;
import fusion.frontend.pipeline.CatalogJson;
import fusion.frontend.pipeline.Json;
import fusion.frontend.pipeline.PipelineException;
import fusion.frontend.pipeline.Severity;
import tools.jackson.databind.JsonNode;

/**
 * Starts {@code fusion} for its short-lived calls (section 10, Calling the backend): with an argument
 * list, never through a shell; JSON read from standard output; killed after a timeout (30 s by
 * default); at most four at once ({@code fusion.backend.max-calls}). They write nothing and make no
 * run directory, so they are outside A1 (1.10).
 */
public final class ProcessBackendClient implements BackendClient {
    /** A call's exit status and output. */
    record Result(int exit, String stdout, String stderr) {}

    private static final Pattern VERSION_LINE = Pattern.compile("^FuSION\\s+(\\S+)");
    private static final int MAX_STDERR = 64 * 1024;

    private final Path executable;
    private final Duration timeout;
    private final Semaphore calls;
    private volatile BackendInfo info;
    private volatile Catalog catalog;
    private volatile String catalogVersion;

    public ProcessBackendClient(Path executable, Duration timeout, int maxCalls) {
        this.executable = executable;
        this.timeout = timeout;
        this.calls = new Semaphore(Math.max(1, maxCalls), true);
    }

    public Path executable() {
        return executable;
    }

    @Override
    public BackendInfo info() {
        BackendInfo i = info;
        if (i == null) {
            i = readInfo();
            info = i;
        }
        return i;
    }

    /** {@code --commands}, cached under fusion's version (section 10, The command catalog). */
    @Override
    public Catalog catalog() {
        String version = info().fusionVersion();
        Catalog c = catalog;
        if (c == null || !version.equals(catalogVersion)) {
            Result r = call(List.of("--commands"), Optional.empty(), null, timeout);
            if (r.exit() != 0) {
                throw new RunnerException.BackendUnavailable("fusion --commands failed (" + r.exit() + "): "
                        + firstLine(r.stderr()));
            }
            try {
                c = CatalogJson.read(r.stdout());
            } catch (PipelineException e) {
                throw new RunnerException.BackendUnavailable("fusion --commands: " + e.getMessage(), e);
            }
            catalog = c;
            catalogVersion = version;
        }
        return c;
    }

    @Override
    public CheckReport check(Path batchFile) {
        Result r = call(List.of("-b", batchFile.toString(), "--check", "--json"), Optional.empty(),
                batchFile.toAbsolutePath().getParent(), timeout);
        if (r.exit() != 0 && r.exit() != 1) {
            throw new RunnerException.BackendUnavailable("fusion --check failed (" + r.exit() + "): "
                    + firstLine(r.stderr()));
        }
        try {
            return parseCheck(Json.parse(r.stdout(), "fusion --check --json"));
        } catch (PipelineException e) {
            throw new RunnerException.BackendUnavailable(e.getMessage(), e);
        }
    }

    @Override
    public JsonNode query(String batchLine, Duration callTimeout) {
        if (batchLine.indexOf('\n') >= 0 || batchLine.indexOf('\r') >= 0) {
            throw new IllegalArgumentException("a query is one batch line");
        }
        Result r = call(List.of("--query"), Optional.of(batchLine + "\n"), null, callTimeout);
        // A refused query still answers on standard output, {"ok": false, "error": {...}}, with status 1.
        JsonNode answer;
        try {
            answer = r.exit() == 0 || r.exit() == 1 ? Json.parse(r.stdout(), "fusion --query") : null;
        } catch (PipelineException e) {
            answer = null;
        }
        if (answer == null) {
            throw new RunnerException.BackendUnavailable("fusion --query failed (" + r.exit() + "): "
                    + firstLine(r.stderr()));
        }
        if (answer.isObject() && answer.has("ok") && !answer.get("ok").asBoolean(true)) {
            JsonNode error = answer.path("error");
            throw new RunnerException.QueryRefused(error.path("code").asString("query.refused"),
                    error.path("message").asString("fusion refused the query " + batchLine));
        }
        if (r.exit() != 0) {
            throw new RunnerException.BackendUnavailable("fusion --query failed (" + r.exit() + "): "
                    + firstLine(r.stderr()));
        }
        return answer;
    }

    // ---- parsing --------------------------------------------------------------------------------

    private BackendInfo readInfo() {
        Result r = call(List.of("--version", "--json"), Optional.empty(), null, timeout);
        if (r.exit() == 0) {
            try {
                return parseInfo(Json.parse(r.stdout(), "fusion --version --json"));
            } catch (PipelineException e) {
                // fall through to the text form
            }
        }
        // A backend without --json still prints its version on the first line (01.C14).
        Result text = call(List.of("--version"), Optional.empty(), null, timeout);
        Matcher m = VERSION_LINE.matcher(text.stdout().strip());
        if (text.exit() != 0 || !m.find()) {
            throw new RunnerException.BackendUnavailable("fusion --version failed (" + r.exit() + "): "
                    + firstLine(r.stderr().isBlank() ? text.stderr() : r.stderr()));
        }
        return new BackendInfo(m.group(1), Map.of(), envHome(), Map.of(), List.of());
    }

    /**
     * Reads {@code --version --json} in section 11's shape: {@code versions.fusion.version},
     * {@code versions.libraries}, {@code install.home}, the data files and the tool probes. Until
     * that lands, it also reads the earlier flat form ({@code fusion}, {@code libraries},
     * {@code home}). Members it does not know are ignored.
     */
    static BackendInfo parseInfo(JsonNode root) {
        JsonNode flatFusion = root.get("fusion");
        String version = Json.text(root.path("versions").path("fusion"), "version")
                .or(() -> flatFusion != null && flatFusion.isString() ? Optional.of(flatFusion.stringValue()) : Optional.empty())
                .or(() -> Json.text(root, "version"))
                .orElseThrow(() -> new PipelineException("json.missing", "fusion --version --json names no version"));
        Map<String, String> libraries = new LinkedHashMap<>();
        JsonNode libs = root.path("versions").has("libraries") ? root.path("versions").get("libraries") : root.path("libraries");
        libs.forEachEntry((k, v) -> libraries.put(k, v.asString()));
        Path home = Json.text(root.path("install"), "home").or(() -> Json.text(root, "home"))
                .map(Path::of).orElseGet(ProcessBackendClient::envHome);
        Map<String, Path> data = new LinkedHashMap<>();
        List<String> failed = new ArrayList<>();
        JsonNode dataNode = root.get("data");
        if (dataNode != null && dataNode.isObject() && dataNode.has("files")) {
            // Section 11's form: the home's files checked against fusion-data.json, and the reference maps.
            dataNode.path("reference_maps").forEachEntry((k, v) -> Json.text(v, "path").ifPresent(p -> data.put(k, Path.of(p))));
            dataNode.path("files").forEachEntry((k, v) -> {
                if (!v.path("ok").asBoolean(true)) {
                    failed.add(k);
                }
            });
        } else if (dataNode != null && dataNode.isObject()) {
            dataNode.forEachEntry((k, v) -> {
                if (v.isString()) {
                    data.put(k, Path.of(v.stringValue()));
                } else if (v.isObject() && v.has("path")) {
                    data.put(k, Path.of(v.get("path").asString()));
                }
            });
        } else if (dataNode != null && dataNode.isArray()) {
            for (JsonNode d : dataNode) {
                Optional<String> path = Json.text(d, "path");
                Optional<String> name = Json.text(d, "name").or(() -> path);
                if (name.isPresent() && path.isPresent()) {
                    data.put(name.get(), Path.of(path.get()));
                }
            }
        }
        root.path("tools").forEachEntry((tool, probe) -> {
            boolean ok = probe.path("ok").asBoolean(true)
                    && Json.text(probe, "status").map(s -> s.equals("ok") || s.equals("optional")).orElse(true);
            if (!ok) {
                failed.add(tool);
            }
        });
        return new BackendInfo(version, libraries, home, data, failed);
    }

    private static Path envHome() {
        String h = System.getenv("FUSION_HOME");
        return h == null || h.isBlank() ? Path.of("") : Path.of(h);
    }

    /** {@code --check --json}: section 01's diagnostics with 09's fields (1.9). */
    static CheckReport parseCheck(JsonNode root) {
        List<CheckDiagnostic> diagnostics = new ArrayList<>();
        for (JsonNode d : root.path("diagnostics")) {
            String sev = Json.text(d, "severity").orElse("error").toLowerCase(Locale.ROOT);
            Severity severity = sev.equals("error") ? Severity.ERROR : Severity.WARNING;
            JsonNode point = d.get("point");
            OptionalInt p = point != null && point.isIntegralNumber() ? OptionalInt.of(point.asInt()) : OptionalInt.empty();
            diagnostics.add(new CheckDiagnostic(severity, d.path("line").asInt(0), p,
                    Json.text(d, "code").orElse("check.unknown"), Json.text(d, "message").orElse(""),
                    Json.text(d, "setting"), Json.text(d, "path")));
        }
        boolean ok = root.path("ok").asBoolean(diagnostics.stream().noneMatch(x -> x.severity() == Severity.ERROR));
        return new CheckReport(ok, diagnostics);
    }

    // ---- processes ------------------------------------------------------------------------------

    Result call(List<String> args, Optional<String> stdin, Path workingDir, Duration callTimeout) {
        if (executable.toString().isEmpty()) {
            throw new RunnerException.BackendUnavailable("the setting fusion.backend.executable names no fusion");
        }
        if (!Files.isRegularFile(executable) || !Files.isExecutable(executable)) {
            throw new RunnerException.BackendUnavailable("no fusion executable at " + executable
                    + " (the setting fusion.backend.executable)");
        }
        try {
            calls.acquire();
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            throw new RunnerException.BackendUnavailable("interrupted while waiting for a fusion call");
        }
        try {
            return run(args, stdin, workingDir, callTimeout);
        } finally {
            calls.release();
        }
    }

    private Result run(List<String> args, Optional<String> stdin, Path workingDir, Duration callTimeout) {
        List<String> command = new ArrayList<>();
        command.add(executable.toString());
        command.addAll(args);
        ProcessBuilder pb = new ProcessBuilder(command);
        if (workingDir != null) {
            pb.directory(workingDir.toFile());
        }
        if (stdin.isEmpty()) {
            pb.redirectInput(ProcessBuilder.Redirect.from(nullDevice()));
        }
        Process p;
        try {
            p = pb.start();
        } catch (IOException e) {
            throw new RunnerException.BackendUnavailable("cannot start " + executable + ": " + e.getMessage(), e);
        }
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        ByteArrayOutputStream err = new ByteArrayOutputStream();
        Thread outReader = Thread.ofVirtual().start(() -> drain(p.getInputStream(), out, Integer.MAX_VALUE));
        Thread errReader = Thread.ofVirtual().start(() -> drain(p.getErrorStream(), err, MAX_STDERR));
        try {
            if (stdin.isPresent()) {
                try (OutputStream in = p.getOutputStream()) {
                    in.write(stdin.get().getBytes(StandardCharsets.UTF_8));
                } catch (IOException e) {
                    // the process ended early; its status says why
                }
            }
            if (!p.waitFor(callTimeout.toMillis(), TimeUnit.MILLISECONDS)) {
                p.descendants().forEach(ProcessHandle::destroyForcibly);
                p.destroyForcibly();
                throw new RunnerException.BackendUnavailable("fusion " + String.join(" ", args) + " took longer than "
                        + callTimeout.toSeconds() + " s and was ended");
            }
            outReader.join();
            errReader.join();
        } catch (InterruptedException e) {
            p.destroyForcibly();
            Thread.currentThread().interrupt();
            throw new RunnerException.BackendUnavailable("interrupted while waiting for fusion");
        }
        return new Result(p.exitValue(), out.toString(StandardCharsets.UTF_8), err.toString(StandardCharsets.UTF_8));
    }

    private static void drain(InputStream in, ByteArrayOutputStream sink, int keep) {
        byte[] buffer = new byte[8192];
        try (in) {
            int n;
            while ((n = in.read(buffer)) > 0) {
                int room = keep - sink.size();
                if (room > 0) {
                    sink.write(buffer, 0, Math.min(room, n));
                }
            }
        } catch (IOException e) {
            // the stream closed with the process
        }
    }

    static File nullDevice() {
        return new File(System.getProperty("os.name").toLowerCase(Locale.ROOT).startsWith("windows") ? "NUL" : "/dev/null");
    }

    private static String firstLine(String text) {
        String t = text.strip();
        int nl = t.indexOf('\n');
        return t.isEmpty() ? "no message" : nl < 0 ? t : t.substring(0, nl);
    }
}
