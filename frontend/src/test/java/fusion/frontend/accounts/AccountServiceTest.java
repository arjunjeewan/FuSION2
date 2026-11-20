package fusion.frontend.accounts;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.attribute.PosixFilePermissions;
import java.time.Clock;
import java.time.Instant;
import java.time.ZoneOffset;
import java.util.Map;
import java.util.Optional;
import java.util.UUID;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.Supplier;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.condition.DisabledOnOs;
import org.junit.jupiter.api.condition.OS;
import org.junit.jupiter.api.io.TempDir;

import fusion.frontend.pipeline.InputValue;
import fusion.frontend.pipeline.Json;
import tools.jackson.databind.JsonNode;

/** Section 10's test hooks for {@code AccountService} and {@code UsernamePolicy} (S8.02, 10.C2, 10.C9, 10.C10). */
class AccountServiceTest {
    @TempDir
    Path dataDir;

    private static final Clock CLOCK = Clock.fixed(Instant.parse("2026-11-12T12:44:05Z"), ZoneOffset.UTC);

    static Supplier<UUID> ids() {
        AtomicInteger n = new AtomicInteger();
        return () -> new UUID(0x6f1d3c2a9b8e4d7fL, 0xa1c23e4f5a6b7c00L + n.incrementAndGet());
    }

    /** Strength 4, bcrypt's least, keeps the tests fast; one test checks the real strength. */
    private FileAccountService fast() {
        return new FileAccountService(dataDir, 4, CLOCK, ids());
    }

    private static char[] pw(String s) {
        return s.toCharArray();
    }

    @Test
    void usernamesAreNormalisedAndChecked() {
        assertEquals("arjun", UsernamePolicy.normalise("  Arjun "));
        for (String ok : java.util.List.of("arjun", "a1", "lab_user-2", "a".repeat(32))) {
            assertTrue(UsernamePolicy.isValid(UsernamePolicy.normalise(ok)), ok);
        }
        for (String bad : java.util.List.of("../x", "a/b", "a", "1abc", "", "a b", "a\\b", "ab.c", "a".repeat(33))) {
            assertFalse(UsernamePolicy.isValid(UsernamePolicy.normalise(bad)), bad);
        }
    }

    @Test
    void hashesAreBcryptAtStrength12AndNameTheirScheme() {
        FileAccountService real = new FileAccountService(dataDir, 12, CLOCK, ids());
        Account a = real.create("Arjun", "Arjun", pw("correct horse"));
        assertTrue(a.passwordHash().startsWith("{bcrypt}$2a$12$"), a.passwordHash());
        assertEquals("arjun", a.username());
        assertEquals(Role.USER, a.role());
    }

    @Test
    void everyAccountIsCreatedWithTheRoleUserAndNoEmail() throws Exception {
        FileAccountService s = fast();
        s.create("arjun", "Arjun", pw("password1"));
        s.create("arpit", "", pw("password2"));
        JsonNode root = Json.parse(Files.readString(dataDir.resolve("accounts.json")), "accounts.json");
        assertEquals(1, root.get("format").asInt());
        for (JsonNode a : root.get("accounts")) {
            assertEquals("user", a.get("role").asString());
            assertFalse(a.has("email"));
            assertTrue(a.get("password_hash").asString().startsWith("{bcrypt}$2a$04$"));
            assertEquals("2026-11-12T12:44:05Z", a.get("created").asString());
        }
        assertEquals("arpit", root.get("accounts").get(1).get("display_name").asString());
    }

    @Test
    void anUnknownUserAndAWrongPasswordGiveTheSameAnswer() {
        FileAccountService s = fast();
        Account a = s.create("arjun", "Arjun", pw("password1"));
        assertEquals(Optional.of(a), s.authenticate(" ARJUN ", pw("password1")));
        assertEquals(Optional.empty(), s.authenticate("arjun", pw("password2")));
        assertEquals(Optional.empty(), s.authenticate("nobody", pw("password1")));
        assertEquals(Optional.empty(), s.authenticate("../etc/passwd", pw("password1")));
    }

    @Test
    void usernamesArePerInstallationUniqueAndPasswordsAre8To72Bytes() {
        FileAccountService s = fast();
        s.create("arjun", "Arjun", pw("password1"));
        assertThrows(AccountException.UsernameTaken.class, () -> s.create("Arjun", "", pw("password1")));
        assertThrows(AccountException.InvalidUsername.class, () -> s.create("../x", "", pw("password1")));
        assertThrows(AccountException.InvalidPassword.class, () -> s.create("bob", "", pw("1234567")));
        assertThrows(AccountException.InvalidPassword.class, () -> s.create("bob", "", pw("x".repeat(73))));
        s.create("carol", "", pw("x".repeat(72)));
        // 24 three-byte characters are 72 bytes; one more is 75.
        s.create("dave", "", pw("€".repeat(24)));
        assertThrows(AccountException.InvalidPassword.class, () -> s.create("erin", "", pw("€".repeat(25))));
        assertThrows(AccountException.InvalidUsername.class, () -> s.create("frank", "bad\nname", pw("password1")));
    }

    @Test
    void theStoreSurvivesARestartAndKeepsPreferences() {
        FileAccountService s = fast();
        Account a = s.create("arjun", "Arjun", pw("password1"));
        Preferences p = new Preferences(Optional.of(Path.of("/home/u/FuSION/runs")),
                Map.of("kgml_dir", new InputValue.PathValue(Path.of("/home/u/kegg"))));
        s.setPreferences(a.id(), p);
        FileAccountService again = new FileAccountService(dataDir, 4, CLOCK, ids());
        Account back = again.find(a.id()).orElseThrow();
        assertEquals(Optional.of(Path.of("/home/u/FuSION/runs")), back.preferences().resultsDir());
        // Remembered values are kept as written; sessions type them by the pipeline's declarations.
        assertEquals("/home/u/kegg", back.preferences().rememberedInputs().get("kgml_dir").text());
        assertTrue(again.authenticate("arjun", pw("password1")).isPresent());
        assertFalse(again.isEmpty());
        assertTrue(new FileAccountService(dataDir.resolve("other"), 4, CLOCK, ids()).isEmpty());
    }

    @Test
    void passwordsChangeAndResetFromTheCommandLine() {
        FileAccountService s = fast();
        Account a = s.create("arjun", "Arjun", pw("password1"));
        assertThrows(AccountException.InvalidPassword.class, () -> s.changePassword(a.id(), pw("wrong-one"), pw("password2")));
        s.changePassword(a.id(), pw("password1"), pw("password2"));
        assertTrue(s.authenticate("arjun", pw("password2")).isPresent());
        s.resetPassword("ARJUN", pw("password3"));
        assertTrue(s.authenticate("arjun", pw("password3")).isPresent());
        assertFalse(s.authenticate("arjun", pw("password2")).isPresent());
        assertThrows(AccountException.UnknownAccount.class, () -> s.resetPassword("nobody", pw("password3")));
    }

    /** A hash of another strength is replaced at the next sign-in (10.C9). */
    @Test
    void anOlderHashIsUpgradedAtTheNextSignIn() {
        FileAccountService weak = fast();
        Account a = weak.create("arjun", "Arjun", pw("password1"));
        FileAccountService stronger = new FileAccountService(dataDir, 5, CLOCK, ids());
        Account upgraded = stronger.authenticate("arjun", pw("password1")).orElseThrow();
        assertTrue(upgraded.passwordHash().startsWith("{bcrypt}$2a$05$"));
        assertEquals(a.id(), upgraded.id());
    }

    @Test
    @DisabledOnOs(OS.WINDOWS)
    void theAccountFileIsOwnerOnly() throws Exception {
        fast().create("arjun", "Arjun", pw("password1"));
        assertEquals("rw-------", PosixFilePermissions.toString(
                Files.getPosixFilePermissions(dataDir.resolve("accounts.json"))));
    }
}
