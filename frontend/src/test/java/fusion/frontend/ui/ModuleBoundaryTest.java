package fusion.frontend.ui;

import static com.tngtech.archunit.lang.syntax.ArchRuleDefinition.noClasses;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;
import java.util.stream.Collectors;

import org.junit.jupiter.api.Test;
import org.springframework.modulith.core.ApplicationModule;
import org.springframework.modulith.core.ApplicationModules;

import com.tngtech.archunit.core.importer.ClassFileImporter;
import com.tngtech.archunit.core.importer.ImportOption;

/**
 * 10.C11: five top-level packages, checked by Spring Modulith's verification, with the dependencies
 * section 10 allows: ui on all the others; runner on pipeline, sessions and accounts; sessions on
 * pipeline and accounts; accounts on pipeline; pipeline on none. JavaFX types appear only in ui, so
 * the four service modules run without a display.
 */
class ModuleBoundaryTest {
    private static final Map<String, Set<String>> ALLOWED = Map.of(
            "ui", Set.of("accounts", "sessions", "pipeline", "runner"),
            "runner", Set.of("pipeline", "sessions", "accounts"),
            "sessions", Set.of("pipeline", "accounts"),
            "accounts", Set.of("pipeline"),
            "pipeline", Set.of());

    @Test
    void fiveModulesWithNoCycleAndNoReachIntoInternals() {
        ApplicationModules modules = ApplicationModules.of("fusion.frontend");
        modules.verify();
        Map<String, Set<String>> actual = new TreeMap<>();
        for (ApplicationModule m : modules) {
            actual.put(m.getIdentifier().toString(), m.getDirectDependencies(modules).uniqueModules()
                    .map(d -> d.getIdentifier().toString()).collect(Collectors.toCollection(TreeSet::new)));
        }
        assertEquals(ALLOWED.keySet(), actual.keySet());
        actual.forEach((module, deps) -> assertTrue(ALLOWED.get(module).containsAll(deps),
                module + " depends on " + deps + ", but may depend only on " + ALLOWED.get(module)));
        // Each allowed edge is used, so the check above sees real dependencies, not an empty analysis.
        Map<String, Set<String>> allowed = new TreeMap<>();
        ALLOWED.forEach((m, d) -> allowed.put(m, new TreeSet<>(d)));
        assertEquals(allowed, actual);
    }

    @Test
    void javaFxStaysInUi() {
        noClasses().that().resideOutsideOfPackage("fusion.frontend.ui..")
                .should().dependOnClassesThat().resideInAPackage("javafx..")
                .check(new ClassFileImporter().withImportOption(new ImportOption.DoNotIncludeTests())
                        .importPackages("fusion.frontend"));
    }
}
