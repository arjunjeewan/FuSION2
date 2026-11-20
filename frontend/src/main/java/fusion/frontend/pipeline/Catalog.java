package fusion.frontend.pipeline;

import java.util.List;
import java.util.Optional;

/**
 * The command catalog that {@code fusion --commands} exports (01.C9): every command with its
 * arguments, rules and effects, and the run-level settings. The frontend keeps no FuSION knowledge
 * of its own (section 10, Principles).
 */
public record Catalog(int format, String fusionVersion, List<CommandSpec> commands, List<SettingSpec> runSettings) {
    public Catalog {
        fusionVersion = fusionVersion == null ? "" : fusionVersion;
        commands = List.copyOf(commands);
        runSettings = List.copyOf(runSettings);
    }

    /** The command of that name or alias, matched exactly (S1.11). */
    public Optional<CommandSpec> command(String nameOrAlias) {
        for (CommandSpec c : commands) {
            if (c.answersTo(nameOrAlias)) {
                return Optional.of(c);
            }
        }
        return Optional.empty();
    }
}
