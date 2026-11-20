package fusion.frontend.pipeline;

import java.util.Objects;
import java.util.Optional;
import java.util.OptionalInt;

/**
 * How to generate a batch.
 *
 * @param stopAfter  a block id: keep only that block and what it depends on (decision 0019)
 * @param sweepValue with {@link Target#RUN}, the index of the swept value to fix (10.C21)
 */
public record GenerateOptions(Target target, Optional<String> stopAfter, OptionalInt sweepValue,
                              Provenance provenance) {
    public GenerateOptions {
        Objects.requireNonNull(target, "target");
        Objects.requireNonNull(stopAfter, "stopAfter");
        Objects.requireNonNull(sweepValue, "sweepValue");
        Objects.requireNonNull(provenance, "provenance");
    }

    public static GenerateOptions run(Provenance provenance) {
        return new GenerateOptions(Target.RUN, Optional.empty(), OptionalInt.empty(), provenance);
    }
}
