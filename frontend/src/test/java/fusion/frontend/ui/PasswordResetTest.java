package fusion.frontend.ui;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.time.Clock;
import java.util.UUID;

import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

import fusion.frontend.accounts.FileAccountService;

/**
 * {@code fusion-frontend --reset-password} with its input piped rather than typed at a terminal:
 * both passwords must come from the one input (Phase 3, prompt log entry 387).
 */
class PasswordResetTest {
    private static final int STRENGTH = 4;   // bcrypt's lowest cost keeps the test quick

    @TempDir
    Path dataDir;

    private InputStream savedIn;
    private String savedStrength;

    @BeforeEach
    void setUp() {
        savedIn = System.in;
        savedStrength = System.getProperty("fusion.accounts.bcrypt-strength");
        System.setProperty("fusion.accounts.bcrypt-strength", Integer.toString(STRENGTH));
        accounts().create("lab", "Lab", "oldpassword1".toCharArray());
    }

    @AfterEach
    void tearDown() {
        System.setIn(savedIn);
        if (savedStrength == null) {
            System.clearProperty("fusion.accounts.bcrypt-strength");
        } else {
            System.setProperty("fusion.accounts.bcrypt-strength", savedStrength);
        }
    }

    private FileAccountService accounts() {
        return new FileAccountService(dataDir, STRENGTH, Clock.systemUTC(), UUID::randomUUID);
    }

    private static void pipe(String text) {
        System.setIn(new ByteArrayInputStream(text.getBytes(StandardCharsets.UTF_8)));
    }

    @Test
    void piped_passwords_reset_the_password() {
        pipe("newpassword1\nnewpassword1\n");
        assertEquals(0, PasswordReset.run("lab", dataDir));
        assertTrue(accounts().authenticate("lab", "newpassword1".toCharArray()).isPresent());
        assertFalse(accounts().authenticate("lab", "oldpassword1".toCharArray()).isPresent());
    }

    @Test
    void piped_passwords_that_differ_change_nothing() {
        pipe("newpassword1\nnewpassword2\n");
        assertEquals(1, PasswordReset.run("lab", dataDir));
        assertTrue(accounts().authenticate("lab", "oldpassword1".toCharArray()).isPresent());
    }

    @Test
    void an_unknown_user_changes_nothing() {
        pipe("newpassword1\nnewpassword1\n");
        assertEquals(1, PasswordReset.run("nobody", dataDir));
        assertTrue(accounts().authenticate("lab", "oldpassword1".toCharArray()).isPresent());
    }
}
