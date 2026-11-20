package fusion.frontend.runner;

import java.nio.file.Path;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

/** What {@code fusion --version --json} reports (section 11): versions, home, reference data, failed probes. */
public record BackendInfo(String fusionVersion, Map<String, String> libraries, Path home,
                          Map<String, Path> referenceData, List<String> failedProbes) {
    public BackendInfo {
        libraries = Collections.unmodifiableMap(new TreeMap<>(libraries));
        referenceData = Collections.unmodifiableMap(new TreeMap<>(referenceData));
        failedProbes = List.copyOf(failedProbes);
    }
}
