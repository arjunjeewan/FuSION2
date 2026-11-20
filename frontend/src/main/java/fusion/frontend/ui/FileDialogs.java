package fusion.frontend.ui;

import java.nio.file.Path;
import java.util.List;
import java.util.Optional;

import javafx.stage.Window;

/**
 * Native file dialogs (10.C7): JavaFX's FileChooser and DirectoryChooser give real paths, opening in
 * the folder of the input's current value or the last folder used for that input.
 */
interface FileDialogs {
    Optional<Path> chooseFile(Window owner, String title, Optional<Path> startIn, List<String> extensions);

    Optional<Path> chooseDirectory(Window owner, String title, Optional<Path> startIn);

    Optional<Path> saveFile(Window owner, String title, Optional<Path> startIn, String initialName);
}
