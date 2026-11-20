package fusion.frontend.pipeline;

import java.util.List;
import java.util.Objects;
import java.util.Optional;

/**
 * One command of the catalog: the editor's block library (S8.19), built from {@code fusion
 * --commands} (01.C9). {@code needs} and {@code produces} are section 01's {@code Effects}.
 */
public record CommandSpec(String name, List<String> aliases, String stage, String summary, String help,
                          List<ArgSpec> args, List<Rule> rules, List<ValueKind> needs,
                          Optional<ValueKind> produces, boolean debug) {
    public CommandSpec {
        Objects.requireNonNull(name, "name");
        aliases = List.copyOf(aliases);
        stage = stage == null ? "" : stage;
        summary = summary == null ? "" : summary;
        help = help == null ? "" : help;
        args = List.copyOf(args);
        rules = List.copyOf(rules);
        needs = List.copyOf(needs);
        Objects.requireNonNull(produces, "produces");
    }

    /** The argument of that bare name, if the command has one. */
    public Optional<ArgSpec> arg(String argName) {
        for (ArgSpec a : args) {
            if (a.name().equals(argName)) {
                return Optional.of(a);
            }
        }
        return Optional.empty();
    }

    /** True if {@code nameOrAlias} names this command exactly (S1.11). */
    public boolean answersTo(String nameOrAlias) {
        return name.equals(nameOrAlias) || aliases.contains(nameOrAlias);
    }
}
