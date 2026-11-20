package fusion.frontend.accounts;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Clock;
import java.time.Instant;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.UUID;
import java.util.concurrent.locks.ReentrantLock;
import java.util.function.Supplier;

import org.springframework.security.crypto.bcrypt.BCryptPasswordEncoder;
import org.springframework.security.crypto.password.DelegatingPasswordEncoder;
import org.springframework.security.crypto.password.PasswordEncoder;

import fusion.frontend.pipeline.AtomicFiles;
import fusion.frontend.pipeline.InputValue;
import fusion.frontend.pipeline.Json;
import fusion.frontend.pipeline.PipelineJson;
import fusion.frontend.pipeline.Wire;
import tools.jackson.databind.JsonNode;
import tools.jackson.databind.node.ArrayNode;
import tools.jackson.databind.node.ObjectNode;

/**
 * The account store: {@code <data dir>/accounts.json}, written by atomic replace with owner-only
 * permissions (10.C8, Security model), with bcrypt at strength 12 through Spring Security's
 * {@link DelegatingPasswordEncoder} (10.C9), so a stored hash names its scheme ({@code {bcrypt}$2a$12$…})
 * and a later scheme replaces it at the next sign-in.
 *
 * <p>FuSION 1 kept a Java-serialised {@code HashMap} in the launch directory, committed to git with
 * an email address and an unsalted SHA-256 over the platform charset, answered "Wrong password!"
 * for an unknown user too, and returned at once for an unknown user
 * ({@code Linux/frontend/src/LoginPage.java:21, 517-561}; S8.02).
 */
public final class FileAccountService implements AccountService {
    static final int FORMAT = 1;
    /** bcrypt reads at most 72 bytes of a password. */
    static final int MAX_PASSWORD_BYTES = 72;
    static final int MIN_PASSWORD_BYTES = 8;

    private final Path file;
    private final Clock clock;
    private final Supplier<UUID> ids;
    private final PasswordEncoder encoder;
    private final ReentrantLock lock = new ReentrantLock();
    private final List<Account> accounts = new ArrayList<>();
    private volatile String dummyHash;

    /**
     * @param strength bcrypt's cost; the setting {@code fusion.accounts.bcrypt-strength} takes 10 to 14
     *                 (12 by default), and tests use a lower one for speed
     */
    public FileAccountService(Path dataDir, int strength, Clock clock, Supplier<UUID> ids) {
        this.file = dataDir.resolve("accounts.json");
        this.clock = clock;
        this.ids = ids;
        this.encoder = new DelegatingPasswordEncoder("bcrypt", Map.of("bcrypt", new BCryptPasswordEncoder(strength)));
        load();
    }

    // ---- the service ----------------------------------------------------------------------------

    @Override
    public Account create(String username, String displayName, char[] password) {
        String name = UsernamePolicy.normalise(username);
        if (!UsernamePolicy.isValid(name)) {
            throw new AccountException.InvalidUsername("a username is 2 to 32 characters: a letter, then letters, "
                    + "digits, _ or -");
        }
        String display = displayName == null || displayName.isBlank() ? name : displayName.strip();
        if (display.length() > 100 || display.codePoints().anyMatch(Character::isISOControl)) {
            throw new AccountException.InvalidUsername("a display name is at most 100 characters, with no control character");
        }
        checkPassword(password);
        String hash = encoder.encode(CharBuffer.wrap(password));
        lock.lock();
        try {
            if (byUsername(name).isPresent()) {
                throw new AccountException.UsernameTaken(name);
            }
            Account a = new Account(new AccountId(ids.get()), name, display, Role.USER, hash,
                    clock.instant(), Preferences.NONE);
            List<Account> next = new ArrayList<>(accounts);
            next.add(a);
            save(next);
            return a;
        } finally {
            lock.unlock();
        }
    }

    @Override
    public Optional<Account> authenticate(String username, char[] password) {
        String name = UsernamePolicy.normalise(username);
        Optional<Account> found;
        lock.lock();
        try {
            found = byUsername(name);
        } finally {
            lock.unlock();
        }
        CharBuffer raw = CharBuffer.wrap(password);
        if (found.isEmpty()) {
            // The same work as a wrong password, so the answer takes as long either way (S8.02).
            encoder.matches(raw, dummyHash());
            return Optional.empty();
        }
        Account a = found.get();
        if (!matches(raw, a.passwordHash())) {
            return Optional.empty();
        }
        if (encoder.upgradeEncoding(a.passwordHash())) {
            // A later scheme, or a changed strength, replaces the hash at its next sign-in (10.C9).
            Account upgraded = withHash(a, encoder.encode(raw));
            replace(upgraded);
            return Optional.of(upgraded);
        }
        return Optional.of(a);
    }

    @Override
    public void changePassword(AccountId id, char[] current, char[] next) {
        Account a = find(id).orElseThrow(() -> new AccountException.UnknownAccount(id.toString()));
        if (!matches(CharBuffer.wrap(current), a.passwordHash())) {
            throw new AccountException.InvalidPassword("the current password is not correct");
        }
        checkPassword(next);
        replace(withHash(a, encoder.encode(CharBuffer.wrap(next))));
    }

    @Override
    public void resetPassword(String username, char[] next) {
        String name = UsernamePolicy.normalise(username);
        checkPassword(next);
        String hash = encoder.encode(CharBuffer.wrap(next));
        lock.lock();
        try {
            Account a = byUsername(name).orElseThrow(() -> new AccountException.UnknownAccount(name));
            replaceLocked(withHash(a, hash));
        } finally {
            lock.unlock();
        }
    }

    @Override
    public void setPreferences(AccountId id, Preferences preferences) {
        lock.lock();
        try {
            Account a = findLocked(id).orElseThrow(() -> new AccountException.UnknownAccount(id.toString()));
            replaceLocked(new Account(a.id(), a.username(), a.displayName(), a.role(), a.passwordHash(),
                    a.created(), preferences));
        } finally {
            lock.unlock();
        }
    }

    @Override
    public Optional<Account> find(AccountId id) {
        lock.lock();
        try {
            return findLocked(id);
        } finally {
            lock.unlock();
        }
    }

    @Override
    public boolean isEmpty() {
        lock.lock();
        try {
            return accounts.isEmpty();
        } finally {
            lock.unlock();
        }
    }

    // ---- helpers --------------------------------------------------------------------------------

    /** 8 to 72 bytes in UTF-8; 72 bytes is bcrypt's limit (section 10, Accounts). */
    static void checkPassword(char[] password) {
        if (password == null) {
            throw new AccountException.InvalidPassword("a password is required");
        }
        ByteBuffer utf8 = StandardCharsets.UTF_8.encode(CharBuffer.wrap(password));
        int length = utf8.remaining();
        if (utf8.hasArray()) {
            Arrays.fill(utf8.array(), (byte) 0);
        }
        if (length < MIN_PASSWORD_BYTES || length > MAX_PASSWORD_BYTES) {
            throw new AccountException.InvalidPassword("a password is 8 to 72 bytes long in UTF-8");
        }
    }

    private boolean matches(CharSequence raw, String hash) {
        try {
            return encoder.matches(raw, hash);
        } catch (IllegalArgumentException e) {
            return false;   // a hash with no known scheme
        }
    }

    private String dummyHash() {
        String h = dummyHash;
        if (h == null) {
            h = encoder.encode("not a password of anyone's");
            dummyHash = h;
        }
        return h;
    }

    private static Account withHash(Account a, String hash) {
        return new Account(a.id(), a.username(), a.displayName(), a.role(), hash, a.created(), a.preferences());
    }

    private Optional<Account> byUsername(String name) {
        return accounts.stream().filter(a -> a.username().equals(name)).findFirst();
    }

    private Optional<Account> findLocked(AccountId id) {
        return accounts.stream().filter(a -> a.id().equals(id)).findFirst();
    }

    private void replace(Account updated) {
        lock.lock();
        try {
            replaceLocked(updated);
        } finally {
            lock.unlock();
        }
    }

    private void replaceLocked(Account updated) {
        List<Account> next = new ArrayList<>(accounts);
        next.replaceAll(a -> a.id().equals(updated.id()) ? updated : a);
        save(next);
    }

    /** Writes the whole store, then swaps it in; a failed write keeps the previous state. */
    private void save(List<Account> next) {
        ObjectNode root = Json.object();
        root.put("format", FORMAT);
        ArrayNode list = root.putArray("accounts");
        for (Account a : next) {
            ObjectNode o = list.addObject();
            o.put("id", a.id().toString());
            o.put("username", a.username());
            o.put("display_name", a.displayName());
            o.put("role", Wire.name(a.role()));
            o.put("password_hash", a.passwordHash());
            o.put("created", a.created().toString());
            ObjectNode p = o.putObject("preferences");
            if (a.preferences().resultsDir().isPresent()) {
                p.put("results_dir", a.preferences().resultsDir().get().toString());
            } else {
                p.putNull("results_dir");
            }
            p.set("remembered_inputs", PipelineJson.writeValues(a.preferences().rememberedInputs()));
        }
        try {
            AtomicFiles.write(file, Json.pretty(root), true);
        } catch (IOException e) {
            throw new AccountException.StoreFailed("cannot write " + file + ": " + e.getMessage(), e);
        }
        accounts.clear();
        accounts.addAll(next);
    }

    private void load() {
        if (!Files.exists(file)) {
            return;
        }
        try {
            JsonNode root = Json.parse(Files.readString(file), file.toString());
            int format = root.path("format").asInt(0);
            if (format != FORMAT) {
                throw new AccountException.StoreFailed(file + ": format " + format + " is not " + FORMAT, null);
            }
            for (JsonNode o : root.path("accounts")) {
                JsonNode p = o.path("preferences");
                Optional<Path> results = Json.text(p, "results_dir").map(Path::of);
                Map<String, InputValue> remembered = PipelineJson.readValues(p.get("remembered_inputs"), List.of());
                accounts.add(new Account(new AccountId(UUID.fromString(Json.requiredText(o, "id", "an account"))),
                        Json.requiredText(o, "username", "an account"),
                        Json.text(o, "display_name").orElse(""),
                        Wire.parse(Role.class, Json.text(o, "role").orElse("user"), "an account's role"),
                        Json.requiredText(o, "password_hash", "an account"),
                        Instant.parse(Json.requiredText(o, "created", "an account")),
                        new Preferences(results, remembered)));
            }
        } catch (IOException | RuntimeException e) {
            if (e instanceof AccountException ae) {
                throw ae;
            }
            throw new AccountException.StoreFailed("cannot read " + file + ": " + e.getMessage(), e);
        }
    }
}
