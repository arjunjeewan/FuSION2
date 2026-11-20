package fusion.frontend.sessions;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.function.Supplier;

import fusion.frontend.pipeline.ArgType;
import fusion.frontend.pipeline.DefaultPipeline;
import fusion.frontend.pipeline.InputDecl;
import fusion.frontend.pipeline.InputHint;
import fusion.frontend.pipeline.InputValue;
import fusion.frontend.pipeline.PipelineDocument;
import fusion.frontend.pipeline.Sha256;

/**
 * The quick checks when a session opens (S8.05): every path-valued input is checked for existence,
 * readability and kind, and every direct input file is hashed and compared with the last run's
 * {@code run.json}, so a screen can say "changed since the last run". An empty file is a valid
 * input, an empty list (decision 0021). FuSION 1 checked no stored path: 27 of the 28 in its
 * shipped sessions no longer existed (S8.05).
 */
public final class FileSessionChecks implements SessionChecks {
    private final DefaultPipeline pipelines;
    private final Supplier<SessionUse> use;

    public FileSessionChecks(DefaultPipeline pipelines, Supplier<SessionUse> use) {
        this.pipelines = pipelines;
        this.use = use;
    }

    @Override
    public List<PathProblem> quick(Session session) {
        PipelineDocument doc = session.pipeline().orElseGet(pipelines::standard);
        Map<Path, String> lastRun = use.get().lastRunInputHashes(session.id());
        List<PathProblem> problems = new ArrayList<>();
        for (InputDecl d : doc.inputs()) {
            if (!d.type().isPath()) {
                continue;
            }
            for (Path p : paths(session.inputs().getOrDefault(d.name(), InputValue.UNSET))) {
                check(d, p, lastRun, problems);
            }
        }
        return problems;
    }

    private static List<Path> paths(InputValue v) {
        List<Path> out = new ArrayList<>();
        switch (v) {
            case InputValue.PathValue p -> out.add(p.value());
            case InputValue.Items items -> items.items().forEach(i -> out.addAll(paths(i)));
            case InputValue.Sweep s -> s.values().forEach(i -> out.addAll(paths(i)));
            // Installation paths are checked by fusion --version --json against fusion-data.json.
            default -> { }
        }
        return out;
    }

    private static void check(InputDecl d, Path p, Map<Path, String> lastRun, List<PathProblem> problems) {
        boolean directory = d.hint().filter(h -> h == InputHint.DIRECTORY).isPresent();
        if (!Files.exists(p)) {
            if (d.type() != ArgType.OUTPUT_NAME) {          // an output folder is made when a run starts
                problems.add(new PathProblem(d.name(), p, PathProblem.Kind.MISSING));
            }
            return;
        }
        if (directory && !Files.isDirectory(p)) {
            problems.add(new PathProblem(d.name(), p, PathProblem.Kind.NOT_A_DIRECTORY));
            return;
        }
        if (!directory && d.type() != ArgType.OUTPUT_NAME && !Files.isRegularFile(p)) {
            problems.add(new PathProblem(d.name(), p, PathProblem.Kind.NOT_A_FILE));
            return;
        }
        if (!Files.isReadable(p)) {
            problems.add(new PathProblem(d.name(), p, PathProblem.Kind.UNREADABLE));
            return;
        }
        String before = lastRun.get(p);
        if (before != null && Files.isRegularFile(p)) {
            try {
                if (!before.equalsIgnoreCase(Sha256.ofFile(p))) {
                    problems.add(new PathProblem(d.name(), p, PathProblem.Kind.CHANGED_SINCE_LAST_RUN));
                }
            } catch (IOException e) {
                problems.add(new PathProblem(d.name(), p, PathProblem.Kind.UNREADABLE));
            }
        }
    }
}
