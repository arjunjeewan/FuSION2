package fusion.frontend.runner;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.UUID;
import java.util.stream.Stream;

import fusion.frontend.pipeline.ArgSpec;
import fusion.frontend.pipeline.ArgType;
import fusion.frontend.pipeline.Binding;
import fusion.frontend.pipeline.CommandSpec;
import fusion.frontend.pipeline.GeneOverride;
import fusion.frontend.pipeline.GeneratedBatch;
import fusion.frontend.pipeline.InputDecl;
import fusion.frontend.pipeline.InputHint;
import fusion.frontend.pipeline.InputValue;
import fusion.frontend.pipeline.PipelineDocument;
import fusion.frontend.pipeline.PipelineProblem;
import fusion.frontend.pipeline.ResolvedPipeline;
import fusion.frontend.pipeline.Severity;
import fusion.frontend.pipeline.SideFile;

/**
 * Validation steps 3 to 5 before a run (S8.30, decision 0021): the gene ids, the paths, and
 * {@code fusion -b <batch> --check --json} (09.C12) on the generated batch in a scratch directory,
 * never a run directory, so a refused Submit leaves no run directory behind. A path missing at step
 * 4 counts as an error only when {@code --check} reports it too, since the batch may make it before
 * reading it (01.C12). The check's {@code batch.undefined_variable} warnings count as errors, for
 * S8.20's stop on any unfilled placeholder.
 */
final class SubmitChecks {
    /** The command and argument that name the symbol map in use (section 02's {@code start}). */
    static final String MAPS_COMMAND = "start";
    static final String SYMBOLS_ARGUMENT = "symbols";

    record PathResult(List<PipelineProblem> errors, Map<String, PipelineProblem> missing) {}

    private final BackendClient backend;
    private final GeneLookup genes;
    private final Path scratchRoot;

    SubmitChecks(BackendClient backend, GeneLookup genes, Path dataDir) {
        this.backend = backend;
        this.genes = genes;
        this.scratchRoot = dataDir.resolve("scratch");
    }

    // ---- step 3: gene ids (S8.12) ---------------------------------------------------------------

    List<PipelineProblem> geneIds(PipelineDocument doc, ResolvedPipeline resolved, List<GeneOverride> overrides) {
        Optional<Path> map = symbolMap(resolved);
        List<PipelineProblem> out = new ArrayList<>();
        try {
            for (InputDecl d : doc.inputs()) {
                if (d.hint().filter(h -> h == InputHint.GENE_ID).isEmpty()) {
                    continue;
                }
                for (String id : ids(resolved.value(d.name()))) {
                    if (!genes.known(map, id)) {
                        out.add(PipelineProblem.forInput(Severity.ERROR, d.name(), "input.unknown_gene",
                                d.label() + ": " + id + " is not in the symbol map"));
                    }
                }
            }
            for (GeneOverride g : overrides) {
                if (!genes.known(map, g.geneId())) {
                    out.add(new PipelineProblem(Severity.ERROR, Optional.empty(), Optional.empty(), Optional.empty(),
                            "override.unknown_gene", "override: " + g.geneId() + " is not in the symbol map"));
                }
            }
        } catch (RunnerException e) {
            // fusion --check still checks every id at its step (05.C3), so this is a warning only.
            out.add(new PipelineProblem(Severity.WARNING, Optional.empty(), Optional.empty(), Optional.empty(),
                    "lookup.unavailable", "gene ids were not checked against the symbol map: " + e.getMessage()));
        }
        return out;
    }

    static Optional<Path> symbolMap(ResolvedPipeline resolved) {
        for (ResolvedPipeline.ResolvedBlock b : resolved.blocks()) {
            if (b.command().name().equals(MAPS_COMMAND) && b.args().containsKey(SYMBOLS_ARGUMENT)) {
                Optional<InputValue> v = resolved.valueOf(b.args().get(SYMBOLS_ARGUMENT));
                if (v.isPresent() && v.get() instanceof InputValue.PathValue p) {
                    return Optional.of(p.value());
                }
            }
        }
        return Optional.empty();
    }

    private static List<String> ids(InputValue v) {
        List<String> out = new ArrayList<>();
        switch (v) {
            case InputValue.GeneId g -> out.add(g.value());
            case InputValue.Text t -> out.add(t.value());
            case InputValue.Items items -> items.items().forEach(i -> out.addAll(ids(i)));
            case InputValue.Sweep s -> s.values().forEach(i -> out.addAll(ids(i)));
            default -> { }
        }
        out.removeIf(String::isBlank);
        return out;
    }

    // ---- step 4: paths --------------------------------------------------------------------------

    PathResult paths(ResolvedPipeline resolved, Optional<Path> home) {
        List<PipelineProblem> errors = new ArrayList<>();
        Map<String, PipelineProblem> missing = new LinkedHashMap<>();
        for (ResolvedPipeline.ResolvedBlock b : resolved.blocks()) {
            for (Map.Entry<String, Binding> e : b.args().entrySet()) {
                ArgSpec spec = b.command().arg(e.getKey()).orElseThrow();
                Optional<String> input = e.getValue() instanceof Binding.Input in ? Optional.of(in.name()) : Optional.empty();
                // An input file by the argument's type, or by the input's own (a folder for a text argument).
                ArgType type = input.flatMap(resolved::input).map(InputDecl::type).orElse(spec.type());
                if (type != ArgType.INPUT_FILE && type != ArgType.INPUT_LIST
                        && spec.type() != ArgType.INPUT_FILE && spec.type() != ArgType.INPUT_LIST) {
                    continue;
                }
                boolean directory = input.flatMap(resolved::input).flatMap(InputDecl::hint)
                        .filter(h -> h == InputHint.DIRECTORY).isPresent();
                for (Path p : paths(resolved.valueOf(e.getValue()).orElse(InputValue.UNSET), home)) {
                    check(b, e.getKey(), input, p, directory, errors, missing);
                }
            }
        }
        return new PathResult(errors, missing);
    }

    private static List<Path> paths(InputValue v, Optional<Path> home) {
        List<Path> out = new ArrayList<>();
        switch (v) {
            case InputValue.PathValue p -> out.add(p.value());
            case InputValue.InstallPath i -> home.ifPresent(h -> out.add(h.resolve(i.relative())));
            case InputValue.Items items -> items.items().forEach(i -> out.addAll(paths(i, home)));
            case InputValue.Sweep s -> s.values().forEach(i -> out.addAll(paths(i, home)));
            default -> { }
        }
        return out;
    }

    private static void check(ResolvedPipeline.ResolvedBlock b, String arg, Optional<String> input, Path p,
                              boolean directory, List<PipelineProblem> errors, Map<String, PipelineProblem> missing) {
        String what = input.map(i -> i + " ").orElse("") + p;
        if (!Files.exists(p)) {
            missing.put(p.toAbsolutePath().normalize().toString(), PipelineProblem.forArgument(Severity.ERROR, b.id(),
                    arg, input, "input.missing", what + " does not exist"));
        } else if (directory && !Files.isDirectory(p)) {
            errors.add(PipelineProblem.forArgument(Severity.ERROR, b.id(), arg, input, "input.not_a_directory",
                    what + " is not a folder"));
        } else if (!directory && !Files.isRegularFile(p)) {
            errors.add(PipelineProblem.forArgument(Severity.ERROR, b.id(), arg, input, "input.not_a_file",
                    what + " is not a file"));
        } else if (!Files.isReadable(p)) {
            errors.add(PipelineProblem.forArgument(Severity.ERROR, b.id(), arg, input, "input.unreadable",
                    what + " cannot be read"));
        }
    }

    // ---- step 5: fusion --check -----------------------------------------------------------------

    List<PipelineProblem> check(GeneratedBatch batch, ResolvedPipeline resolved, Map<String, PipelineProblem> missing) {
        Path dir = scratchRoot.resolve(UUID.randomUUID().toString());
        try {
            Files.createDirectories(dir);
            Path file = dir.resolve("run.batch");
            Files.writeString(file, batch.text());
            for (SideFile f : batch.sideFiles()) {
                Path side = dir.resolve(f.relativePath());
                Files.createDirectories(side.getParent());
                Files.writeString(side, f.text());
            }
            CheckReport report = backend.check(file);
            return map(report, batch, resolved, missing);
        } catch (IOException e) {
            throw new RunnerException("run.scratch", "cannot write the batch to check in " + dir + ": " + e.getMessage());
        } finally {
            deleteTree(dir);
        }
    }

    static List<PipelineProblem> map(CheckReport report, GeneratedBatch batch, ResolvedPipeline resolved,
                                     Map<String, PipelineProblem> missing) {
        List<PipelineProblem> out = new ArrayList<>();
        for (CheckDiagnostic d : report.diagnostics()) {
            Optional<PipelineProblem> ours = d.path().map(p -> missing.get(Path.of(p).toAbsolutePath().normalize().toString()));
            if (ours.isPresent() && ours.get() != null) {
                out.add(ours.get());
                continue;
            }
            Severity severity = d.severity() == Severity.ERROR || d.code().equals("batch.undefined_variable")
                    ? Severity.ERROR : Severity.WARNING;
            Optional<String> block = batch.sourceMap().block(d.line());
            Optional<String> argument = Optional.empty();
            if (block.isPresent() && d.setting().isPresent()) {
                Optional<CommandSpec> c = resolved.blocks().stream().filter(x -> x.id().equals(block.get()))
                        .map(ResolvedPipeline.ResolvedBlock::command).findFirst();
                argument = c.flatMap(cs -> cs.args().stream().filter(a -> a.setting().equals(d.setting().get()))
                        .map(ArgSpec::name).findFirst());
            }
            Optional<String> input = argument.flatMap(a -> batch.sourceMap().input(d.line(), a))
                    .or(() -> batch.sourceMap().input(d.line(), ""));
            out.add(new PipelineProblem(severity, block, argument, input, d.code(),
                    d.message().isEmpty() ? d.code() : d.message()));
        }
        return out;
    }

    private static void deleteTree(Path dir) {
        if (!Files.exists(dir)) {
            return;
        }
        try (Stream<Path> walk = Files.walk(dir)) {
            for (Path p : walk.sorted(Comparator.reverseOrder()).toList()) {
                Files.deleteIfExists(p);
            }
        } catch (IOException e) {
            // scratch/ is safe to delete by hand; nothing depends on it
        }
    }
}
