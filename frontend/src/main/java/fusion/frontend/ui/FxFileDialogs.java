package fusion.frontend.ui;

import java.io.File;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.Optional;

import javafx.stage.DirectoryChooser;
import javafx.stage.FileChooser;
import javafx.stage.Window;

/** {@link FileDialogs} through JavaFX's native choosers (10.C7). */
final class FxFileDialogs implements FileDialogs {
    @Override
    public Optional<Path> chooseFile(Window owner, String title, Optional<Path> startIn, List<String> extensions) {
        FileChooser c = new FileChooser();
        c.setTitle(title);
        folder(startIn).ifPresent(c::setInitialDirectory);
        if (!extensions.isEmpty()) {
            c.getExtensionFilters().add(new FileChooser.ExtensionFilter(String.join(", ", extensions),
                    extensions.stream().map(e -> "*" + e).toList()));
            c.getExtensionFilters().add(new FileChooser.ExtensionFilter("All files", "*"));
        }
        return Optional.ofNullable(c.showOpenDialog(owner)).map(File::toPath);
    }

    @Override
    public Optional<Path> chooseDirectory(Window owner, String title, Optional<Path> startIn) {
        DirectoryChooser c = new DirectoryChooser();
        c.setTitle(title);
        folder(startIn).ifPresent(c::setInitialDirectory);
        return Optional.ofNullable(c.showDialog(owner)).map(File::toPath);
    }

    @Override
    public Optional<Path> saveFile(Window owner, String title, Optional<Path> startIn, String initialName) {
        FileChooser c = new FileChooser();
        c.setTitle(title);
        folder(startIn).ifPresent(c::setInitialDirectory);
        c.setInitialFileName(initialName);
        return Optional.ofNullable(c.showSaveDialog(owner)).map(File::toPath);
    }

    /** The folder of the current value, or the value itself when it is a folder. */
    private static Optional<File> folder(Optional<Path> startIn) {
        return startIn.map(p -> Files.isDirectory(p) ? p : p.getParent()).filter(p -> p != null && Files.isDirectory(p))
                .map(Path::toFile);
    }
}
