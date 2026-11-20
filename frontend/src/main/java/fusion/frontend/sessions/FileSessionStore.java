package fusion.frontend.sessions;

import java.io.IOException;
import java.nio.file.DirectoryStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.text.Normalizer;
import java.time.Clock;
import java.time.Instant;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.locks.ReentrantLock;
import java.util.function.Supplier;
import java.util.function.UnaryOperator;
import java.util.stream.Stream;

import fusion.frontend.accounts.Account;
import fusion.frontend.accounts.AccountId;
import fusion.frontend.accounts.AccountService;
import fusion.frontend.pipeline.AtomicFiles;
import fusion.frontend.pipeline.DefaultPipeline;
import fusion.frontend.pipeline.GeneOverride;
import fusion.frontend.pipeline.InputDecl;
import fusion.frontend.pipeline.InputValue;
import fusion.frontend.pipeline.Json;
import fusion.frontend.pipeline.PipelineDocument;
import fusion.frontend.pipeline.PipelineException;
import fusion.frontend.pipeline.PipelineJson;
import fusion.frontend.pipeline.SideFile;
import tools.jackson.databind.JsonNode;
import tools.jackson.databind.node.ObjectNode;

/**
 * The session store (10.C8): one folder per session, {@code <data dir>/sessions/<id>/}, with
 * {@code session.json} written by atomic replace and {@code overrides/<kind>.txt} rewritten from the
 * record on every change, for section 04's preview. Every change states the version it was based on
 * (VersionConflict), names are checked before anything is created, and a busy session cannot be
 * deleted.
 *
 * <p>FuSION 1 kept one JSON array per user, addressed sessions by list position, created a session
 * on Cancel or a blank name, checked duplicates on the untrimmed name, dropped null keys and did not
 * store the overrides ({@code Linux/frontend/src/SessionFrame.java:196-268},
 * {@code UserInput.java:344-407}; S8.04, S8.05).
 */
public final class FileSessionStore implements SessionStore {
    static final int FORMAT = 1;
    static final int MAX_NAME = 100;
    private static final System.Logger LOG = System.getLogger(FileSessionStore.class.getName());

    /** A stored session; its inputs are typed when read, by the pipeline it runs. */
    private record Entry(SessionId id, AccountId owner, String name, Instant created, Instant updated,
                         long version, JsonNode inputs, List<GeneOverride> overrides,
                         Optional<PipelineDocument> pipeline) {}

    private final Path root;
    private final DefaultPipeline pipelines;
    private final AccountService accounts;
    private final Supplier<SessionUse> use;
    private final Clock clock;
    private final Supplier<UUID> ids;
    private final ReentrantLock lock = new ReentrantLock();
    private final Map<SessionId, Entry> index = new HashMap<>();

    /** @param use the runner's answer to "is this session busy", looked up when needed (10.C11) */
    public FileSessionStore(Path dataDir, DefaultPipeline pipelines, AccountService accounts,
                            Supplier<SessionUse> use, Clock clock, Supplier<UUID> ids) {
        this.root = dataDir.resolve("sessions");
        this.pipelines = pipelines;
        this.accounts = accounts;
        this.use = use;
        this.clock = clock;
        this.ids = ids;
        load();
    }

    // ---- the store ------------------------------------------------------------------------------

    @Override
    public List<SessionSummary> list(AccountId owner) {
        lock.lock();
        try {
            return index.values().stream().filter(e -> e.owner().equals(owner))
                    .sorted(Comparator.comparing(Entry::updated).reversed()
                            .thenComparing(Entry::name).thenComparing(e -> e.id().toString()))
                    .map(e -> new SessionSummary(e.id(), e.name(), e.created(), e.updated()))
                    .toList();
        } finally {
            lock.unlock();
        }
    }

    @Override
    public Session create(AccountId owner, String name) {
        String clean = checkName(name);
        Map<String, InputValue> inputs = initialInputs(owner);
        lock.lock();
        try {
            refuseTakenName(owner, clean, Optional.empty());
            Instant now = clock.instant();
            Session s = new Session(new SessionId(ids.get()), owner, clean, now, now, 1, inputs, List.of(),
                    Optional.empty());
            write(s);
            return s;
        } finally {
            lock.unlock();
        }
    }

    @Override
    public Optional<Session> get(AccountId owner, SessionId id) {
        Entry e;
        lock.lock();
        try {
            e = index.get(id);
        } finally {
            lock.unlock();
        }
        return e == null || !e.owner().equals(owner) ? Optional.empty() : Optional.of(decode(e));
    }

    @Override
    public Session update(AccountId owner, SessionId id, long basedOnVersion, UnaryOperator<Session> change) {
        lock.lock();
        try {
            Session current = get(owner, id).orElseThrow(() -> new SessionException.SessionNotFound(id.toString()));
            if (current.version() != basedOnVersion) {
                throw new SessionException.VersionConflict(basedOnVersion, current.version());
            }
            Session changed = change.apply(current);
            String name = changed.name().equals(current.name()) ? current.name() : checkName(changed.name());
            if (!name.equals(current.name())) {
                refuseTakenName(owner, name, Optional.of(id));
            }
            List<GeneOverride> overrides = checkOverrides(changed.overrides());
            Session next = new Session(current.id(), current.owner(), name, current.created(), clock.instant(),
                    current.version() + 1, changed.inputs(), overrides, changed.pipeline());
            write(next);
            return next;
        } finally {
            lock.unlock();
        }
    }

    @Override
    public void delete(AccountId owner, SessionId id) {
        lock.lock();
        try {
            Entry e = index.get(id);
            if (e == null || !e.owner().equals(owner)) {
                throw new SessionException.SessionNotFound(id.toString());
            }
            if (use.get().busy(id)) {
                throw new SessionException.SessionBusy("a run of this session is queued or running; stop it first. "
                        + "Run folders are never deleted and stay in the results folder.");
            }
            deleteTree(root.resolve(id.toString()));
            index.remove(id);
        } finally {
            lock.unlock();
        }
    }

    @Override
    public Optional<Path> overrideFile(AccountId owner, SessionId id, String kind) {
        lock.lock();
        try {
            Entry e = index.get(id);
            if (e == null || !e.owner().equals(owner) || !GeneOverride.KINDS.contains(kind)) {
                return Optional.empty();
            }
            Path f = root.resolve(id.toString()).resolve("overrides").resolve(kind + ".txt");
            return Files.isRegularFile(f) ? Optional.of(f) : Optional.empty();
        } finally {
            lock.unlock();
        }
    }

    // ---- names, inputs and overrides ------------------------------------------------------------

    /**
     * Trimmed, 1 to 100 characters, no control character; refused names create nothing, so Cancel
     * and blank names never open a session (S8.04). The name is kept in Unicode NFC.
     */
    static String checkName(String raw) {
        String name = raw == null ? "" : Normalizer.normalize(raw.strip(), Normalizer.Form.NFC);
        if (name.isEmpty()) {
            throw new SessionException.InvalidSessionName("a session needs a name");
        }
        if (name.codePointCount(0, name.length()) > MAX_NAME) {
            throw new SessionException.InvalidSessionName("a session's name is at most " + MAX_NAME + " characters");
        }
        boolean control = name.codePoints().anyMatch(cp -> {
            int type = Character.getType(cp);
            return type == Character.CONTROL || type == Character.LINE_SEPARATOR
                    || type == Character.PARAGRAPH_SEPARATOR;
        });
        if (control) {
            throw new SessionException.InvalidSessionName("a session's name cannot hold a line break or a control character");
        }
        return name;
    }

    /** Unique per owner, ignoring case, after NFC normalisation. */
    static String nameKey(String name) {
        return Normalizer.normalize(name, Normalizer.Form.NFC).toLowerCase(Locale.ROOT);
    }

    private void refuseTakenName(AccountId owner, String name, Optional<SessionId> except) {
        String key = nameKey(name);
        for (Entry e : index.values()) {
            if (e.owner().equals(owner) && nameKey(e.name()).equals(key)
                    && (except.isEmpty() || !except.get().equals(e.id()))) {
                throw new SessionException.SessionNameTaken(name);
            }
        }
    }

    /**
     * A new session's values: each declared input's default (S8.06), then the owner's remembered
     * values for inputs marked {@code remember} (S8.31).
     */
    private Map<String, InputValue> initialInputs(AccountId owner) {
        Map<String, InputValue> inputs = new LinkedHashMap<>();
        List<InputDecl> decls = standardDecls();
        Map<String, InputValue> remembered = accounts.find(owner).map(Account::preferences)
                .map(p -> p.rememberedInputs()).orElse(Map.of());
        for (InputDecl d : decls) {
            InputValue v = d.initialValue();
            if (d.remember() && remembered.containsKey(d.name())) {
                try {
                    v = PipelineJson.readValue(PipelineJson.writeValue(remembered.get(d.name())), d);
                } catch (PipelineException e) {
                    LOG.log(System.Logger.Level.WARNING, "remembered value of " + d.name() + " ignored: " + e.getMessage());
                }
            }
            inputs.put(d.name(), v);
        }
        return inputs;
    }

    /** At most one row per gene, and each a bare id; rows with no override are dropped (S8.15). */
    private static List<GeneOverride> checkOverrides(List<GeneOverride> overrides) {
        List<GeneOverride> kept = new ArrayList<>();
        Set<String> genes = new HashSet<>();
        for (GeneOverride g : overrides) {
            if (g.isEmpty()) {
                continue;
            }
            GeneOverride stripped = new GeneOverride(g.geneId().strip(), g.observation(), g.activity(), g.relaxation());
            if (!genes.add(stripped.geneId())) {
                throw new SessionException.InvalidChange("gene " + stripped.geneId() + " has two override rows");
            }
            kept.add(stripped);
        }
        try {
            GeneOverride.idsByKind(kept);
        } catch (PipelineException e) {
            throw new SessionException.InvalidChange(e.getMessage());
        }
        return kept;
    }

    private List<InputDecl> standardDecls() {
        try {
            return pipelines.standard().inputs();
        } catch (RuntimeException e) {
            LOG.log(System.Logger.Level.WARNING, "the default pipeline is not available: " + e.getMessage());
            return List.of();
        }
    }

    private Session decode(Entry e) {
        List<InputDecl> decls = e.pipeline().map(PipelineDocument::inputs).orElseGet(this::standardDecls);
        Map<String, InputValue> inputs;
        try {
            inputs = PipelineJson.readValues(e.inputs(), decls);
        } catch (PipelineException ex) {
            // A value that no longer fits its declaration is kept as written, for the checks to report.
            LOG.log(System.Logger.Level.WARNING, "session " + e.id() + ": " + ex.getMessage());
            inputs = PipelineJson.readValues(e.inputs(), List.of());
        }
        return new Session(e.id(), e.owner(), e.name(), e.created(), e.updated(), e.version(), inputs,
                e.overrides(), e.pipeline());
    }

    // ---- files ----------------------------------------------------------------------------------

    private void write(Session s) {
        Path dir = root.resolve(s.id().toString());
        ObjectNode o = Json.object();
        o.put("format", FORMAT);
        o.put("id", s.id().toString());
        o.put("owner", s.owner().toString());
        o.put("name", s.name());
        o.put("created", s.created().toString());
        o.put("updated", s.updated().toString());
        o.put("version", s.version());
        o.set("inputs", PipelineJson.writeValues(s.inputs()));
        o.set("overrides", PipelineJson.writeOverrides(s.overrides()));
        if (s.pipeline().isPresent()) {
            o.set("pipeline", PipelineJson.write(s.pipeline().get()));
        } else {
            o.putNull("pipeline");
        }
        try {
            AtomicFiles.ensureDirectory(root, true);
            AtomicFiles.ensureDirectory(dir, true);
            AtomicFiles.write(dir.resolve("session.json"), Json.pretty(o), true);
            writeOverrideFiles(dir.resolve("overrides"), s.overrides());
        } catch (IOException e) {
            throw new SessionException.StoreFailed("cannot write session " + s.id() + ": " + e.getMessage(), e);
        }
        index.put(s.id(), new Entry(s.id(), s.owner(), s.name(), s.created(), s.updated(), s.version(),
                o.get("inputs"), s.overrides(), s.pipeline()));
    }

    /** {@code overrides/<kind>.txt}, one id per line, for kinds that have genes; the others are removed. */
    private static void writeOverrideFiles(Path dir, List<GeneOverride> overrides) throws IOException {
        Map<String, List<String>> byKind = GeneOverride.idsByKind(overrides);
        for (String kind : GeneOverride.KINDS) {
            Path f = dir.resolve(kind + ".txt");
            List<String> ids = byKind.get(kind);
            if (ids == null) {
                Files.deleteIfExists(f);
            } else {
                AtomicFiles.write(f, new SideFile("overrides/" + kind + ".txt", ids).text(), true);
            }
        }
    }

    private void load() {
        if (!Files.isDirectory(root)) {
            return;
        }
        try (DirectoryStream<Path> dirs = Files.newDirectoryStream(root)) {
            for (Path dir : dirs) {
                Path f = dir.resolve("session.json");
                if (!Files.isRegularFile(f)) {
                    continue;
                }
                try {
                    Entry e = read(Json.parse(Files.readString(f), f.toString()));
                    index.put(e.id(), e);
                } catch (IOException | RuntimeException e) {
                    LOG.log(System.Logger.Level.WARNING, "skipped " + f + ": " + e.getMessage());
                }
            }
        } catch (IOException e) {
            throw new SessionException.StoreFailed("cannot read " + root + ": " + e.getMessage(), e);
        }
    }

    private static Entry read(JsonNode o) {
        int format = o.path("format").asInt(0);
        if (format != FORMAT) {
            // A later format is converted on load by code; none exists yet.
            throw new PipelineException("session.format", "session format " + format + " is not " + FORMAT);
        }
        JsonNode pipeline = o.get("pipeline");
        return new Entry(new SessionId(UUID.fromString(Json.requiredText(o, "id", "a session"))),
                new AccountId(UUID.fromString(Json.requiredText(o, "owner", "a session"))),
                Json.requiredText(o, "name", "a session"),
                Instant.parse(Json.requiredText(o, "created", "a session")),
                Instant.parse(Json.requiredText(o, "updated", "a session")),
                o.path("version").asLong(1),
                o.get("inputs") == null ? Json.object() : o.get("inputs"),
                PipelineJson.readOverrides(o.get("overrides")),
                pipeline == null || pipeline.isNull() ? Optional.empty() : Optional.of(PipelineJson.read(pipeline)));
    }

    private static void deleteTree(Path dir) {
        if (!Files.exists(dir)) {
            return;
        }
        try (Stream<Path> walk = Files.walk(dir)) {
            for (Path p : walk.sorted(Comparator.reverseOrder()).toList()) {
                Files.delete(p);
            }
        } catch (IOException e) {
            throw new SessionException.StoreFailed("cannot delete " + dir + ": " + e.getMessage(), e);
        }
    }
}
