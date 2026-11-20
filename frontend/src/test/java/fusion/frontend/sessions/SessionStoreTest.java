package fusion.frontend.sessions;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.attribute.PosixFilePermissions;
import java.time.Clock;
import java.time.Duration;
import java.time.Instant;
import java.time.ZoneId;
import java.time.ZoneOffset;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.Supplier;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.condition.DisabledOnOs;
import org.junit.jupiter.api.condition.OS;
import org.junit.jupiter.api.io.TempDir;

import fusion.frontend.accounts.Account;
import fusion.frontend.accounts.AccountService;
import fusion.frontend.accounts.FileAccountService;
import fusion.frontend.accounts.Preferences;
import fusion.frontend.pipeline.Activity;
import fusion.frontend.pipeline.DefaultPipeline;
import fusion.frontend.pipeline.GeneOverride;
import fusion.frontend.pipeline.InputValue;
import fusion.frontend.pipeline.Json;
import fusion.frontend.pipeline.Observation;
import fusion.frontend.pipeline.PipelineDocument;
import fusion.frontend.pipeline.Sha256;
import fusion.frontend.pipeline.TestFixtures;
import tools.jackson.databind.JsonNode;

/** Section 10's test hooks for {@code SessionStore} and the quick checks (S8.04, S8.05, 10.C8). */
class SessionStoreTest {
    @TempDir
    Path dataDir;

    /** A clock that moves one second per reading, so changes have distinct times. */
    static final class StepClock extends Clock {
        private Instant now = Instant.parse("2026-11-12T12:40:11Z");

        @Override
        public ZoneId getZone() {
            return ZoneOffset.UTC;
        }

        @Override
        public Clock withZone(ZoneId zone) {
            return this;
        }

        @Override
        public synchronized Instant instant() {
            now = now.plus(Duration.ofSeconds(1));
            return now;
        }
    }

    static Supplier<UUID> ids() {
        AtomicInteger n = new AtomicInteger();
        return () -> new UUID(0x0b6f2c1e6a4d4f0eL, 0x9d1a5c3e8f2b7a00L + n.incrementAndGet());
    }

    /** A runner stand-in: which sessions are busy, and last-run hashes. */
    static final class FakeUse implements SessionUse {
        final Set<SessionId> busy = new HashSet<>();
        final Map<Path, String> hashes = new HashMap<>();

        @Override
        public boolean busy(SessionId session) {
            return busy.contains(session);
        }

        @Override
        public Map<Path, String> lastRunInputHashes(SessionId session) {
            return hashes;
        }
    }

    private final DefaultPipeline standard = TestFixtures::standard;
    private final FakeUse use = new FakeUse();
    private AccountService accounts;
    private Account arjun;
    private Account arpit;

    @BeforeEach
    void accounts() {
        accounts = new FileAccountService(dataDir, 4, Clock.systemUTC(), UUID::randomUUID);
        arjun = accounts.create("arjun", "Arjun", "password1".toCharArray());
        arpit = accounts.create("arpit", "Arpit", "password1".toCharArray());
    }

    private FileSessionStore store() {
        return new FileSessionStore(dataDir, standard, accounts, () -> use, new StepClock(), ids());
    }

    @Test
    void createRenameAndAStaleVersionIsAConflict() {
        FileSessionStore s = store();
        Session a = s.create(arjun.id(), "  EGFR to CXCL8, FSC JUN ");
        assertEquals("EGFR to CXCL8, FSC JUN", a.name());
        assertEquals(1, a.version());
        // A new session starts with the default pipeline's defaults (S8.06).
        assertEquals(new InputValue.Int(10), a.inputs().get("bound"));
        assertEquals(InputValue.UNSET, a.inputs().get("up"));
        Session b = s.update(arjun.id(), a.id(), 1, x -> x.withName("EGFR to CXCL8, b10"));
        assertEquals(2, b.version());
        assertTrue(b.updated().isAfter(a.updated()));
        assertThrows(SessionException.VersionConflict.class,
                () -> s.update(arjun.id(), a.id(), 1, x -> x.withInput("up", TestFixtures.real(1.01))));
        Session c = s.update(arjun.id(), a.id(), 2, x -> x.withInput("up", TestFixtures.real(1.01)));
        assertEquals(TestFixtures.real(1.01), c.inputs().get("up"));
        // Read back by a new store, from the files alone.
        Session again = store().get(arjun.id(), a.id()).orElseThrow();
        assertEquals(c, again);
        // Another user sees nothing of it.
        assertEquals(Optional.empty(), store().get(arpit.id(), a.id()));
        assertEquals(List.of(), store().list(arpit.id()));
    }

    @Test
    void anInputSetToNullKeepsItsKey() throws Exception {
        FileSessionStore s = store();
        Session a = s.create(arjun.id(), "one");
        s.update(arjun.id(), a.id(), 1, x -> x.withInput("merged_graph", InputValue.UNSET));
        JsonNode json = Json.parse(Files.readString(dataDir.resolve("sessions/" + a.id() + "/session.json")), "s");
        assertTrue(json.get("inputs").has("merged_graph"));
        assertTrue(json.get("inputs").get("merged_graph").isNull());
        assertTrue(json.get("pipeline").isNull());
        assertEquals(1, json.get("format").asInt());
    }

    @Test
    void aCrashBetweenTheTemporaryFileAndTheRenameLeavesTheOldRecord() throws Exception {
        FileSessionStore s = store();
        Session a = s.create(arjun.id(), "one");
        Path dir = dataDir.resolve("sessions/" + a.id());
        // What a crash leaves: a half-written temporary file beside the record.
        Files.writeString(dir.resolve(".session.json.12345.tmp"), "{\"format\": 1, \"id\": \"tru");
        Session back = store().get(arjun.id(), a.id()).orElseThrow();
        assertEquals(a, back);
    }

    @Test
    void blankDuplicateOrMultiLineNamesCreateNothing() {
        FileSessionStore s = store();
        s.create(arjun.id(), "EGFR run");
        assertThrows(SessionException.InvalidSessionName.class, () -> s.create(arjun.id(), "   "));
        assertThrows(SessionException.InvalidSessionName.class, () -> s.create(arjun.id(), null));
        assertThrows(SessionException.InvalidSessionName.class, () -> s.create(arjun.id(), "two\nlines"));
        assertThrows(SessionException.InvalidSessionName.class, () -> s.create(arjun.id(), "tab\there"));
        assertThrows(SessionException.InvalidSessionName.class, () -> s.create(arjun.id(), "x".repeat(101)));
        // FuSION 1 checked the untrimmed name, so " a" passed beside "a" (SessionFrame.java:198).
        assertThrows(SessionException.SessionNameTaken.class, () -> s.create(arjun.id(), " egfr RUN "));
        // Names compare after NFC: "é" composed equals "e" with a combining accent.
        s.create(arjun.id(), "café");
        assertThrows(SessionException.SessionNameTaken.class, () -> s.create(arjun.id(), "café"));
        assertEquals(2, s.list(arjun.id()).size());
        // Another owner may use the same name.
        s.create(arpit.id(), "EGFR run");
        assertEquals(2, store().list(arjun.id()).size());
    }

    @Test
    void deleteIsRefusedWhileARunOfTheSessionIsBusy() {
        FileSessionStore s = store();
        Session a = s.create(arjun.id(), "one");
        use.busy.add(a.id());
        assertThrows(SessionException.SessionBusy.class, () -> s.delete(arjun.id(), a.id()));
        assertTrue(Files.exists(dataDir.resolve("sessions/" + a.id())));
        use.busy.clear();
        assertThrows(SessionException.SessionNotFound.class, () -> s.delete(arpit.id(), a.id()));
        s.delete(arjun.id(), a.id());
        assertFalse(Files.exists(dataDir.resolve("sessions/" + a.id())));
        assertEquals(List.of(), store().list(arjun.id()));
    }

    @Test
    void listsAreNewestChangeFirstThenName() {
        FileSessionStore s = store();
        Session a = s.create(arjun.id(), "alpha");
        Session b = s.create(arjun.id(), "beta");
        s.create(arjun.id(), "gamma");
        s.update(arjun.id(), a.id(), 1, x -> x.withInput("up", TestFixtures.real(1)));
        assertEquals(List.of("alpha", "gamma", "beta"), s.list(arjun.id()).stream().map(SessionSummary::name).toList());
        assertEquals(b.id(), s.list(arjun.id()).get(2).id());
    }

    @Test
    void newSessionsCopyTheOwnersRememberedInputs() {
        accounts.setPreferences(arjun.id(), new Preferences(Optional.empty(),
                Map.of("kgml_dir", new InputValue.Text("/home/u/kegg"), "up", TestFixtures.real(9))));
        Session a = store().create(arjun.id(), "one");
        // Typed by its declaration: a directory path. Only inputs marked remember are copied.
        assertEquals(new InputValue.PathValue(Path.of("/home/u/kegg")), a.inputs().get("kgml_dir"));
        assertEquals(InputValue.UNSET, a.inputs().get("up"));
    }

    @Test
    void overridesAreStoredAndTheirFilesRewrittenOnEveryChange() throws Exception {
        FileSessionStore s = store();
        Session a = s.create(arjun.id(), "one");
        GeneOverride fosb = new GeneOverride("hsa2354", Optional.of(Observation.NOT_DE), Optional.empty(), Optional.empty());
        GeneOverride jun = new GeneOverride("hsa3725", Optional.of(Observation.CONFIRMED_UP),
                Optional.of(Activity.ACTIVE), Optional.empty());
        s.update(arjun.id(), a.id(), 1, x -> x.withOverrides(List.of(fosb, jun)));
        Path dir = dataDir.resolve("sessions/" + a.id() + "/overrides");
        assertEquals("hsa2354\n", Files.readString(dir.resolve("not_de.txt")));
        assertEquals("hsa3725\n", Files.readString(dir.resolve("active.txt")));
        assertEquals(List.of(fosb, jun), store().get(arjun.id(), a.id()).orElseThrow().overrides());
        s.update(arjun.id(), a.id(), 2, x -> x.withOverrides(List.of(jun)));
        assertFalse(Files.exists(dir.resolve("not_de.txt")));
        assertThrows(SessionException.InvalidChange.class,
                () -> s.update(arjun.id(), a.id(), 3, x -> x.withOverrides(List.of(jun, jun))));
        assertThrows(SessionException.InvalidChange.class, () -> s.update(arjun.id(), a.id(), 3,
                x -> x.withOverrides(List.of(new GeneOverride("hsa1 hsa2", Optional.of(Observation.NOT_DE),
                        Optional.empty(), Optional.empty())))));
    }

    @Test
    void anEditedPipelineIsStoredWithItsOrigin() {
        FileSessionStore s = store();
        Session a = s.create(arjun.id(), "one");
        PipelineDocument std = TestFixtures.standard();
        PipelineDocument edited = new PipelineDocument(std.format(), std.origin(), std.inputs(),
                std.blocks().stream().filter(b -> !b.id().equals("explain")).toList());
        s.update(arjun.id(), a.id(), 1, x -> x.withPipeline(Optional.of(edited)));
        Session back = store().get(arjun.id(), a.id()).orElseThrow();
        assertEquals(Optional.of(edited), back.pipeline());
        assertEquals("standard@0.1.0", back.pipeline().orElseThrow().origin());
    }

    @Test
    @DisabledOnOs(OS.WINDOWS)
    void sessionFilesAreOwnerOnly() throws Exception {
        Session a = store().create(arjun.id(), "one");
        assertEquals("rw-------", PosixFilePermissions.toString(Files.getPosixFilePermissions(
                dataDir.resolve("sessions/" + a.id() + "/session.json"))));
        assertEquals("rwx------", PosixFilePermissions.toString(Files.getPosixFilePermissions(
                dataDir.resolve("sessions"))));
    }

    // ---- the quick checks (S8.05) ----

    @Test
    void quickChecksFindMissingWrongKindAndChangedInputs() throws Exception {
        FileSessionStore s = store();
        Session a = s.create(arjun.id(), "one");
        Path lfc = Files.writeString(dataDir.resolve("lfc.tsv"), "hsa1\t1.5\n");
        Path empty = Files.writeString(dataDir.resolve("not_merged.txt"), "");
        Path kgml = Files.createDirectory(dataDir.resolve("kgml"));
        Session b = s.update(arjun.id(), a.id(), 1, x -> x
                .withInput("lfc", new InputValue.PathValue(lfc))
                .withInput("not_merged", new InputValue.PathValue(empty))         // empty is fine (0021)
                .withInput("kgml_dir", new InputValue.PathValue(kgml))
                .withInput("merged_graph", new InputValue.PathValue(kgml))            // a folder, not a file
                .withInput("symbols", new InputValue.PathValue(dataDir.resolve("gone.tsv")))
                .withInput("coexpression_csv", new InputValue.PathValue(lfc))
                .withInput("results_dir", new InputValue.PathValue(dataDir.resolve("runs-to-come"))));
        use.hashes.put(lfc, Sha256.ofFile(lfc));
        FileSessionChecks checks = new FileSessionChecks(standard, () -> use);
        assertEquals(List.of(
                new PathProblem("merged_graph", kgml, PathProblem.Kind.NOT_A_FILE),
                new PathProblem("symbols", dataDir.resolve("gone.tsv"), PathProblem.Kind.MISSING)), checks.quick(b));
        Files.writeString(lfc, "hsa1\t1.6\n");
        List<PathProblem> after = checks.quick(b);
        assertTrue(after.contains(new PathProblem("lfc", lfc, PathProblem.Kind.CHANGED_SINCE_LAST_RUN)), after.toString());
        Session c = s.update(arjun.id(), a.id(), 2, x -> x.withInput("kgml_dir", new InputValue.PathValue(lfc)));
        assertTrue(checks.quick(c).contains(new PathProblem("kgml_dir", lfc, PathProblem.Kind.NOT_A_DIRECTORY)));
    }
}
