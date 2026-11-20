package fusion.frontend.ui;

import java.io.BufferedReader;
import java.io.Console;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.time.Clock;
import java.util.Arrays;
import java.util.Optional;
import java.util.UUID;

import fusion.frontend.accounts.AccountException;
import fusion.frontend.accounts.FileAccountService;

/**
 * {@code fusion-frontend --reset-password <username>} (section 10, Accounts; 10.C10): there is no
 * email, so a reset runs from the command line, by someone with access to the OS login. It refuses
 * while the application is running, whose own copy of the accounts would undo the reset.
 */
final class PasswordReset {
    private PasswordReset() {}

    static int run(String username, Path dataDir) {
        try {
            Optional<SingleInstance> lock = SingleInstance.claim(dataDir);
            if (lock.isEmpty()) {
                System.err.println("FuSION is running for " + dataDir + "; quit it, then reset the password.");
                return 1;
            }
            SingleInstance held = lock.get();
            try {
                char[] first = read("New password for " + username + ": ");
                char[] second = read("The same again: ");
                try {
                    if (!Arrays.equals(first, second)) {
                        System.err.println("The two passwords differ; nothing was changed.");
                        return 1;
                    }
                    int strength = Integer.getInteger("fusion.accounts.bcrypt-strength", 12);
                    new FileAccountService(dataDir, strength, Clock.systemUTC(), UUID::randomUUID)
                            .resetPassword(username, first);
                    System.out.println("The password of " + username + " was reset.");
                    return 0;
                } finally {
                    Arrays.fill(first, '\0');
                    Arrays.fill(second, '\0');
                }
            } finally {
                held.close();
            }
        } catch (AccountException e) {
            System.err.println(e.getMessage());
            return 1;
        } catch (IOException e) {
            System.err.println("cannot use " + dataDir + ": " + e.getMessage());
            return 1;
        }
    }

    private static char[] read(String prompt) throws IOException {
        Console console = System.console();
        if (console != null) {
            char[] p = console.readPassword("%s", prompt);
            return p == null ? new char[0] : p;
        }
        System.out.print(prompt);
        String line = new BufferedReader(new InputStreamReader(System.in, StandardCharsets.UTF_8)).readLine();
        return line == null ? new char[0] : line.toCharArray();
    }
}
