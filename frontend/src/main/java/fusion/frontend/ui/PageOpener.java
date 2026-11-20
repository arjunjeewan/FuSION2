package fusion.frontend.ui;

import java.nio.file.Path;

/** Opens section 07's pages, and run folders, in the system browser or file manager (10.C6). */
interface PageOpener {
    void open(Path page);
}
