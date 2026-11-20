package fusion.frontend.ui;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.concurrent.TimeUnit;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.condition.DisabledOnOs;
import org.junit.jupiter.api.condition.OS;
import org.junit.jupiter.api.io.TempDir;

/** Section 10's ui test hooks that need no screen: the FX hand-over, the single instance, the folders. */
class UiServicesTest {
    @TempDir
    Path tmp;

    /** 1,000 messages in one tick reach the screens in one hand-over (10.C5). */
    @Test
    void oneThousandUpdatesInOneTickAreOneHandOver() {
        List<Runnable> handOvers = new ArrayList<>();
        CoalescingFxDispatch fx = new CoalescingFxDispatch(handOvers::add);
        List<Integer> applied = new ArrayList<>();
        for (int i = 0; i < 1000; i++) {
            int n = i;
            fx.post(() -> applied.add(n));
        }
        assertEquals(1, handOvers.size());
        handOvers.get(0).run();
        assertEquals(1000, applied.size());
        assertEquals(999, applied.get(999));
        // The next tick is a new hand-over.
        fx.post(() -> applied.add(-1));
        assertEquals(2, handOvers.size());
    }

    @Test
    void aSecondInstanceAsksTheFirstToComeForward() throws Exception {
        Optional<SingleInstance> first = SingleInstance.claim(tmp);
        assertTrue(first.isPresent());
        assertTrue(SingleInstance.claim(tmp).isEmpty());          // in this JVM too
        assertFalse(first.get().focusRequested());
        SingleInstance.askToFocus(tmp);
        assertTrue(first.get().focusRequested());
        assertFalse(first.get().focusRequested());                // seen once, then deleted
        first.get().close();
        Optional<SingleInstance> again = SingleInstance.claim(tmp);
        assertTrue(again.isPresent());
        again.get().close();
    }

    /** Another process sees the lock, as a second launch would, and writes frontend.focus (10.C4). */
    @Test
    @DisabledOnOs(OS.WINDOWS)
    void anotherProcessSeesTheLock() throws Exception {
        Optional<SingleInstance> held = SingleInstance.claim(tmp);
        assertTrue(held.isPresent());
        String cp = Path.of(SingleInstance.class.getProtectionDomain().getCodeSource().getLocation().toURI())
                + java.io.File.pathSeparator
                + Path.of(SecondLaunch.class.getProtectionDomain().getCodeSource().getLocation().toURI());
        Process p = new ProcessBuilder(Path.of(System.getProperty("java.home"), "bin", "java").toString(),
                "-XX:TieredStopAtLevel=1", "-cp", cp, SecondLaunch.class.getName(), tmp.toString())
                .redirectErrorStream(true).start();
        assertTrue(p.waitFor(20, TimeUnit.SECONDS));
        assertEquals("held\n", new String(p.getInputStream().readAllBytes()));
        assertTrue(held.get().focusRequested());
        held.get().close();
    }

    @Test
    void dataAndResultsFoldersFollowEachPlatform() {
        Path home = Path.of("/home/u");
        assertEquals(Path.of("/home/u/.local/share/fusion/frontend"), DataDirectory.dataDir(Map.of(), "Linux", home));
        assertEquals(Path.of("/xdg/fusion/frontend"), DataDirectory.dataDir(Map.of("XDG_DATA_HOME", "/xdg"), "Linux", home));
        assertEquals(Path.of("/home/u/Library/Application Support/FuSION/frontend"),
                DataDirectory.dataDir(Map.of(), "Mac OS X", home));
        assertEquals(Path.of("C:/Users/u/AppData/Local", "FuSION", "frontend"),
                DataDirectory.dataDir(Map.of("LOCALAPPDATA", "C:/Users/u/AppData/Local"), "Windows 11", home));
        assertEquals(Path.of("/set"), DataDirectory.dataDir(Map.of("FUSION_DATA_DIR", "/set"), "Linux", home));
        assertEquals(Path.of("/home/u/FuSION/runs"), DataDirectory.resultsDir(Map.of(), home));
        assertTrue(Files.isDirectory(tmp));
    }
}
