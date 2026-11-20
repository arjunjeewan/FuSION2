package fusion.frontend.runner;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Duration;
import java.util.List;
import java.util.Map;
import java.util.Optional;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.condition.DisabledOnOs;
import org.junit.jupiter.api.condition.OS;
import org.junit.jupiter.api.io.TempDir;

import fusion.frontend.pipeline.Json;
import fusion.frontend.pipeline.Severity;
import fusion.frontend.pipeline.TestFixtures;

/**
 * The short-lived {@code fusion} calls (section 10, Calling the backend: 09.C12, 04.C8, 10.C15),
 * against {@link FakeFusion}: an argument list, never a shell; JSON on standard output; a timeout.
 */
@DisabledOnOs(OS.WINDOWS)
class BackendClientTest {
    @TempDir
    Path tmp;

    private Path fake(Map<String, String> env) throws Exception {
        Path catalog = tmp.resolve("catalog.json");
        Files.writeString(catalog, TestFixtures.catalogText());
        Path labels = tmp.resolve("labels.json");
        Files.writeString(labels, "{\"pairs\": [{\"id\": \"hsa1956\", \"label\": \"EGFR\"}, [\"hsa3576\", \"CXCL8\"],"
                + " {\"id\": \"hsa1027\", \"symbol\": \"CDKN1B\"}, {\"id\": \"hsa1956x\", \"label\": \"EGFR2\"}]}");
        java.util.Map<String, String> all = new java.util.HashMap<>(env);
        all.putIfAbsent("FAKE_FUSION_CATALOG", catalog.toString());
        all.putIfAbsent("FAKE_FUSION_LABELS", labels.toString());
        all.putIfAbsent("FAKE_FUSION_HOME", tmp.resolve("home").toString());
        return FakeFusion.launcher(Files.createTempDirectory(tmp, "bin"), all);
    }

    @Test
    void versionCatalogCheckAndQuery() throws Exception {
        ProcessBackendClient c = new ProcessBackendClient(fake(Map.of()), Duration.ofSeconds(20), 4);
        BackendInfo info = c.info();
        assertEquals("0.1.0", info.fusionVersion());
        assertEquals(tmp.resolve("home"), info.home());
        assertEquals("fb_rch", c.catalog().command("corridor").orElseThrow().name());
        Path batch = Files.writeString(tmp.resolve("run.batch"), "exit\n");
        CheckReport ok = c.check(batch);
        assertTrue(ok.ok());
        GeneLookup genes = new GeneLookup(c, Duration.ofSeconds(20), tmp.resolve("data"));
        assertTrue(genes.known(Optional.empty(), "hsa3576"));
        assertEquals(List.of(new GeneLookup.Gene("hsa1956", "EGFR"), new GeneLookup.Gene("hsa1956x", "EGFR2")),
                genes.search(Optional.empty(), "egfr"));
        assertEquals("CDKN1B", genes.search(Optional.empty(), "hsa1027").get(0).label());
        // The answer is cached on disk, keyed by fusion's version, the request and the map (10.C15).
        try (var files = Files.list(tmp.resolve("data/query-cache"))) {
            assertEquals(1, files.count());
        }
    }

    @Test
    void aRefusedQueryIsFusionsAnswerAndAFailedOneIsNot() throws Exception {
        // fusion refuses a query with exit 1 and a JSON answer naming its code (04.C8).
        Path refused = Files.writeString(tmp.resolve("cudf.json"), "{\"ok\": false, \"query\": \"cudf\", \"error\":"
                + " {\"code\": \"input.missing\", \"message\": \"cannot read /x/egf.tsv: no such file\"}}");
        ProcessBackendClient c = new ProcessBackendClient(fake(Map.of("FAKE_FUSION_CUDF", refused.toString())),
                Duration.ofSeconds(20), 4);
        RunnerException.QueryRefused e = assertThrows(RunnerException.QueryRefused.class,
                () -> c.query("cudf values=\"/x/egf.tsv\" up=1 down=-1", Duration.ofSeconds(20)));
        assertEquals("input.missing", e.backendCode());
        assertEquals("cannot read /x/egf.tsv: no such file", e.getMessage());
        // A failure with no answer on standard output means the backend could not be asked.
        RunnerException.BackendUnavailable u = assertThrows(RunnerException.BackendUnavailable.class,
                () -> c.query("suggest_thresholds values=\"/x/egf.tsv\"", Duration.ofSeconds(20)));
        assertTrue(u.getMessage().contains("cannot answer suggest_thresholds"), u.getMessage());
    }

    @Test
    void aBackendWithoutJsonStillGivesItsVersion() throws Exception {
        ProcessBackendClient c = new ProcessBackendClient(fake(Map.of("FAKE_FUSION_NO_JSON", "1",
                "FAKE_FUSION_VERSION", "0.1.0")), Duration.ofSeconds(20), 4);
        assertEquals("0.1.0", c.info().fusionVersion());
    }

    @Test
    void checkDiagnosticsAreRead() throws Exception {
        Path answer = tmp.resolve("check.json");
        Files.writeString(answer, """
                {"ok": false, "batch": "/x/run.batch", "points": 1, "diagnostics": [
                 {"severity": "error", "line": 18, "column": 7, "point": null, "code": "input.missing",
                  "command": "cudf", "setting": "expression.values", "path": "/x/egf.tsv", "message": "missing"},
                 {"severity": "note", "line": 3, "column": 1, "point": 2, "code": "batch.no_fsc",
                  "command": null, "setting": null, "path": null, "message": "no FSC"}],
                 "features": []}""");
        ProcessBackendClient c = new ProcessBackendClient(fake(Map.of("FAKE_FUSION_CHECK", answer.toString())),
                Duration.ofSeconds(20), 4);
        CheckReport r = c.check(Files.writeString(tmp.resolve("run.batch"), "exit\n"));
        assertEquals(false, r.ok());
        assertEquals(Severity.ERROR, r.diagnostics().get(0).severity());
        assertEquals(Optional.of("expression.values"), r.diagnostics().get(0).setting());
        assertEquals(Optional.of("/x/egf.tsv"), r.diagnostics().get(0).path());
        assertEquals(Severity.WARNING, r.diagnostics().get(1).severity());
        assertEquals(java.util.OptionalInt.of(2), r.diagnostics().get(1).point());
    }

    @Test
    void aSlowCallIsEndedAndAMissingExecutableIsReported() throws Exception {
        ProcessBackendClient slow = new ProcessBackendClient(fake(Map.of("FAKE_FUSION_SLEEP_MS", "20000")),
                Duration.ofMillis(800), 4);
        RunnerException.BackendUnavailable e = assertThrows(RunnerException.BackendUnavailable.class, slow::info);
        assertTrue(e.getMessage().contains("took longer than"), e.getMessage());
        ProcessBackendClient none = new ProcessBackendClient(tmp.resolve("nowhere/fusion"), Duration.ofSeconds(1), 4);
        e = assertThrows(RunnerException.BackendUnavailable.class, none::info);
        assertTrue(e.getMessage().contains("fusion.backend.executable"), e.getMessage());
    }

    @Test
    void versionJsonInSection11sShape() {
        BackendInfo info = ProcessBackendClient.parseInfo(Json.parse("""
                {"versions": {"fusion": {"version": "2.0.0", "git_commit": "3f2c"},
                              "libraries": {"z3": "5.3.3.0", "pugixml": "1.16"}},
                 "install": {"prefix": "/opt/fusion/lib/app/backend", "home": "/opt/fusion/lib/app/backend/share/fusion"},
                 "data": {"symbols": {"path": "/opt/fusion/share/fusion/data/hsa_to_gene_symbol_map.txt"}},
                 "tools": {"gnuplot": {"version": "6.0.5", "ok": true}, "dot": {"status": "optional"},
                           "sh": {"status": "missing"}}}""", "version"));
        assertEquals("2.0.0", info.fusionVersion());
        assertEquals("5.3.3.0", info.libraries().get("z3"));
        assertEquals(Path.of("/opt/fusion/lib/app/backend/share/fusion"), info.home());
        assertEquals(Path.of("/opt/fusion/share/fusion/data/hsa_to_gene_symbol_map.txt"), info.referenceData().get("symbols"));
        assertEquals(List.of("sh"), info.failedProbes());
    }
}
