package fusion.frontend.runner;

import java.util.Locale;
import java.util.Optional;

/**
 * How a run's end maps to its state (section 10, Errors; 01.C14's exit statuses): 0 finished, 1 and
 * 2 failed, 3 and 130 stopped, a Kill killed, and any other status without a final record crashed:
 * a POSIX death by signal n as 128 + n (134 an abort, 137 SIGKILL, 139 a segmentation fault), or a
 * Windows status code such as {@code 0xC0000005}. FuSION 1 ignored the exit status, so a run that
 * aborted still ended "Execution Completed" ({@code Linux/frontend/src/Gui4Panel.java:174-180}; S8.28).
 */
public final class ExitMapping {
    private ExitMapping() {}

    /**
     * @param killed      the runner ended the process (10.C19)
     * @param finalStatus the {@code status} of {@code run.json} when it is final: ok, failed or stopped
     */
    public static RunState state(int status, boolean killed, Optional<String> finalStatus) {
        if (killed) {
            return RunState.KILLED;
        }
        switch (status) {
            case 0:
                return RunState.FINISHED;
            case 1:
            case 2:
                return RunState.FAILED;
            case 3:
            case 130:
                return RunState.STOPPED;
            default:
                return finalStatus.map(ExitMapping::fromRunJson).orElse(RunState.CRASHED);
        }
    }

    /** A final {@code run.json} status: ok, failed or stopped; anything else is not final. */
    public static RunState fromRunJson(String status) {
        return switch (status) {
            case "ok" -> RunState.FINISHED;
            case "failed" -> RunState.FAILED;
            case "stopped" -> RunState.STOPPED;
            default -> throw new IllegalArgumentException("not a final status: " + status);
        };
    }

    public static boolean isFinal(String runJsonStatus) {
        return runJsonStatus.equals("ok") || runJsonStatus.equals("failed") || runJsonStatus.equals("stopped");
    }

    /** The status in words, for the screens. */
    public static String describe(int status, boolean windows) {
        if (windows && status < 0) {
            return String.format(Locale.ROOT, "Windows status 0x%08X", status);
        }
        if (!windows && status > 128 && status < 128 + 65) {
            int signal = status - 128;
            String name = switch (signal) {
                case 6 -> " (SIGABRT, an abort)";
                case 9 -> " (SIGKILL; also the out-of-memory killer)";
                case 11 -> " (SIGSEGV, a segmentation fault)";
                case 15 -> " (SIGTERM)";
                default -> "";
            };
            return "ended by signal " + signal + name;
        }
        return "exit status " + status;
    }
}
