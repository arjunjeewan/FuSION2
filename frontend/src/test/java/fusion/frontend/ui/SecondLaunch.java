package fusion.frontend.ui;

import java.nio.file.Path;

/** What a second launch does, in its own process: find the lock held, and ask for the window (10.C4). */
public final class SecondLaunch {
    private SecondLaunch() {}

    public static void main(String[] args) throws Exception {
        Path dataDir = Path.of(args[0]);
        var claimed = SingleInstance.claim(dataDir);
        if (claimed.isPresent()) {
            claimed.get().close();
            System.out.print("claimed\n");
        } else {
            SingleInstance.askToFocus(dataDir);
            System.out.print("held\n");
        }
    }
}
