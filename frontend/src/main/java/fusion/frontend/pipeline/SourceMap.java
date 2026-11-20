package fusion.frontend.pipeline;

import java.util.Collections;
import java.util.Map;
import java.util.Optional;
import java.util.TreeMap;

/**
 * Where each line of a generated batch came from, so the backend's diagnostics, keyed by line and
 * argument, map back to blocks and inputs (S8.30). A {@code let} line maps under the argument "".
 */
public record SourceMap(Map<Integer, String> blockByLine, Map<String, String> inputByLineAndArgument) {
    public SourceMap {
        blockByLine = Collections.unmodifiableMap(new TreeMap<>(blockByLine));
        inputByLineAndArgument = Collections.unmodifiableMap(new TreeMap<>(inputByLineAndArgument));
    }

    /** The key of {@link #inputByLineAndArgument}: {@code "<line>:<argument>"}. */
    public static String key(int line, String argument) {
        return line + ":" + argument;
    }

    public Optional<String> block(int line) {
        return Optional.ofNullable(blockByLine.get(line));
    }

    public Optional<String> input(int line, String argument) {
        return Optional.ofNullable(inputByLineAndArgument.get(key(line, argument)));
    }
}
