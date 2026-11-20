package fusion.frontend.ui;

import java.io.IOException;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.OptionalInt;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

import javafx.collections.FXCollections;
import javafx.geometry.Insets;
import javafx.scene.Parent;
import javafx.scene.control.Button;
import javafx.scene.control.CheckBox;
import javafx.scene.control.ComboBox;
import javafx.scene.control.Label;
import javafx.scene.control.TextArea;
import javafx.scene.layout.BorderPane;
import javafx.scene.layout.HBox;
import javafx.scene.layout.VBox;

import fusion.frontend.pipeline.Block;
import fusion.frontend.pipeline.Catalog;
import fusion.frontend.pipeline.Condition;
import fusion.frontend.pipeline.GenerateOptions;
import fusion.frontend.pipeline.GeneratedBatch;
import fusion.frontend.pipeline.PipelineDocument;
import fusion.frontend.pipeline.Provenance;
import fusion.frontend.pipeline.SideFile;
import fusion.frontend.pipeline.Target;
import fusion.frontend.runner.SubmitRequest;
import fusion.frontend.sessions.Session;

/**
 * The session's pipeline, with minimal editing for this first version (decision 0038; Phase 5
 * designs the drawn editor): each block of the default pipeline can be left out, the edit is kept in
 * the session with its origin, and Reset goes back to the default (S8.18). Submit can stop after a
 * block (decision 0019). Preview shows the batch a run gets, and Export writes it as a file, or a zip
 * with its {@code overrides/} folder (Gen Script, S8.19). FuSION 1's editor never affected a run, and
 * its Gen Script wrote a template that nothing ran ({@code gui4.java:4086-4511}; S8.18, S8.19).
 */
final class PipelinePane {
    private final SessionView view;
    private final BorderPane root = new BorderPane();
    private final VBox blocks = new VBox(4);
    private final Map<String, CheckBox> included = new LinkedHashMap<>();
    private final ComboBox<String> stopAfter = new ComboBox<>();
    private final TextArea preview = new TextArea();
    private final Label origin = Ui.muted("");
    private final Label status = Ui.muted("");

    PipelinePane(SessionView view) {
        this.view = view;
        Button apply = new Button("Keep these blocks");
        apply.setOnAction(e -> apply());
        Button reset = new Button("Reset to the default pipeline");
        reset.setOnAction(e -> view.change(s -> s.withPipeline(Optional.empty()), s -> status.setText("Reset.")));
        Button submit = new Button("Submit, stopping after");
        submit.setOnAction(e -> {
            if (stopAfter.getValue() != null) {
                view.submit(new SubmitRequest(Optional.of(stopAfter.getValue())),
                        p -> status.setText(SessionView.submitted(p)));
            }
        });
        Button showBatch = new Button("Preview the batch");
        showBatch.setOnAction(e -> generate(Target.RUN, Optional.empty()));
        Button export = new Button("Export the batch...");
        export.setOnAction(e -> export());
        preview.setEditable(false);
        preview.setStyle("-fx-font-family: monospace;");
        VBox top = new VBox(8, origin, Ui.muted("Blocks run in order; a block's condition can leave it out of a run."),
                blocks, new HBox(8, apply, reset), new HBox(8, submit, stopAfter), new HBox(8, showBatch, export), status);
        top.setPadding(new Insets(12));
        root.setTop(top);
        root.setCenter(preview);
        refresh();
    }

    Parent root() {
        return root;
    }

    void refresh() {
        PipelineDocument std = view.standard();
        PipelineDocument doc = view.document();
        origin.setText(view.session().pipeline().isPresent() ? "An edited copy of " + doc.origin()
                : "The default pipeline, " + std.origin());
        blocks.getChildren().clear();
        included.clear();
        for (Block b : std.blocks()) {
            CheckBox box = new CheckBox(b.id() + " · " + b.command() + stage(b) + when(b));
            box.setSelected(doc.block(b.id()).isPresent());
            included.put(b.id(), box);
            blocks.getChildren().add(box);
        }
        stopAfter.setItems(FXCollections.observableArrayList(doc.blocks().stream().map(Block::id).toList()));
    }

    private String stage(Block b) {
        return view.catalog().flatMap(c -> c.command(b.command())).map(c -> c.stage().isEmpty() ? "" : " (" + c.stage() + ")")
                .orElse("");
    }

    private static String when(Block b) {
        if (b.when().isEmpty()) {
            return "";
        }
        List<String> parts = new ArrayList<>();
        for (Condition c : b.when()) {
            parts.add(c.input() + c.equals().map(v -> " = " + v.text()).orElse(" is set"));
        }
        return "  when " + String.join(" and ", parts);
    }

    /** The edited copy keeps the default's inputs and origin, without the blocks left out (S8.18). */
    private void apply() {
        PipelineDocument std = view.standard();
        List<Block> kept = std.blocks().stream().filter(b -> included.get(b.id()).isSelected()).toList();
        Optional<PipelineDocument> edited = kept.size() == std.blocks().size() ? Optional.empty()
                : Optional.of(new PipelineDocument(std.format(), std.origin(), std.inputs(), kept));
        view.change(s -> s.withPipeline(edited), s -> status.setText("Saved."));
    }

    private void generate(Target target, Optional<Path> exportTo) {
        Optional<Catalog> catalog = view.catalog();
        if (catalog.isEmpty()) {
            status.setText("The backend is not available, so no batch can be written.");
            return;
        }
        Session session = view.session();
        PipelineDocument doc = view.document();
        Provenance prov = new Provenance(fusion.frontend.pipeline.FrontendVersion.current(), session.id().toString());
        OptionalInt point = target == Target.RUN
                && session.inputs().values().stream().anyMatch(v -> v instanceof fusion.frontend.pipeline.InputValue.Sweep)
                ? OptionalInt.of(0) : OptionalInt.empty();
        Background.run(() -> {
            GeneratedBatch b = view.services().generator().generate(doc, session.inputs(), session.overrides(),
                    catalog.get(), new GenerateOptions(target, Optional.empty(), point, prov));
            if (exportTo.isPresent()) {
                write(exportTo.get(), b);
            }
            return b;
        }, b -> {
            preview.setText(b.text());
            status.setText(exportTo.map(p -> "Written to " + p).orElse(point.isPresent()
                    ? "The batch of the sweep's first point." : ""));
        }, e -> status.setText(Ui.describe(e)));
    }

    /** Gen Script (S8.19): the batch alone, or a zip of the batch and its overrides/ when it has overrides. */
    private void export() {
        boolean zip = !view.session().overrides().isEmpty();
        String name = view.session().id() + (zip ? ".zip" : ".batch");
        view.services().dialogs().saveFile(view.owner(), "Export the batch", Optional.empty(), name)
                .ifPresent(p -> generate(Target.EXPORT, Optional.of(p)));
    }

    private static void write(Path to, GeneratedBatch b) {
        try {
            if (b.sideFiles().isEmpty()) {
                Files.writeString(to, b.text(), StandardCharsets.UTF_8);
                return;
            }
            try (OutputStream out = Files.newOutputStream(to); ZipOutputStream zip = new ZipOutputStream(out)) {
                zip.putNextEntry(new ZipEntry("run.batch"));
                zip.write(b.text().getBytes(StandardCharsets.UTF_8));
                zip.closeEntry();
                for (SideFile f : b.sideFiles()) {
                    zip.putNextEntry(new ZipEntry(f.relativePath()));
                    zip.write(f.text().getBytes(StandardCharsets.UTF_8));
                    zip.closeEntry();
                }
            }
        } catch (IOException e) {
            throw new java.io.UncheckedIOException(e);
        }
    }
}
