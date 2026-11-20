// FuSION 2.0's desktop frontend (section 10): a JavaFX application (10.C1) with Spring Boot as its
// application framework and no web server (11.C21). Section 11 lays the build out (11.C13, 11.C25).

import org.springframework.boot.gradle.plugin.SpringBootPlugin

plugins {
    java
    application
    alias(libs.plugins.spring.boot)
}

group = "fusion"
// One VERSION file serves the backend and the frontend (11.C16).
version = layout.projectDirectory.file("../VERSION").asFile.readText().trim()

java {
    // Language version 21, any vendor (11.C13).
    toolchain {
        languageVersion = JavaLanguageVersion.of(21)
    }
}

// OpenJFX's jars carry native code, so each platform takes its own classifier (11.C25).
val javafxClassifiers = listOf("linux", "linux-aarch64", "mac", "mac-aarch64", "win")
val javafxPlatform: String = run {
    val os = System.getProperty("os.name").lowercase()
    val arm = System.getProperty("os.arch").lowercase().let { it == "aarch64" || it == "arm64" }
    when {
        os.contains("win") -> "win"
        os.contains("mac") -> if (arm) "mac-aarch64" else "mac"
        else -> if (arm) "linux-aarch64" else "linux"
    }
}
val javafxModules = listOf(libs.javafx.base, libs.javafx.graphics, libs.javafx.controls)

// Resolved only to record every platform's JavaFX jars in verification-metadata.xml, so a build on
// macOS or Windows verifies its own classifier too (11.C4).
val javafxPlatforms = configurations.dependencyScope("javafxPlatforms")
configurations.resolvable("javafxAllPlatforms") {
    extendsFrom(javafxPlatforms.get())
}

dependencies {
    implementation(platform(SpringBootPlugin.BOM_COORDINATES))
    implementation(libs.spring.boot.starter)
    implementation(libs.spring.security.crypto)
    implementation(libs.jackson.databind)
    // The jars are declared directly, without their poms' platform profiles (11.C25).
    for (module in javafxModules) {
        implementation(variantOf(module) { classifier(javafxPlatform) }) { isTransitive = false }
        for (platform in javafxClassifiers) {
            "javafxPlatforms"(variantOf(module) { classifier(platform) }) { isTransitive = false }
        }
    }

    testImplementation(libs.junit.jupiter)
    testImplementation(libs.spring.modulith.core)
    testRuntimeOnly(libs.junit.platform.launcher)
}

application {
    mainClass = "fusion.frontend.ui.Launcher"
    applicationDefaultJvmArgs = listOf("-Xmx512m")   // section 10, Memory
}

tasks.withType<JavaCompile>().configureEach {
    options.encoding = "UTF-8"
    options.release = 21
    options.compilerArgs.addAll(listOf("-Xlint:all", "-Werror"))
}

tasks.processResources {
    val frontendVersion = project.version.toString()
    inputs.property("version", frontendVersion)
    filesMatching("fusion/frontend/version.properties") {
        expand("version" to frontendVersion)
    }
}

// A plain jar, built reproducibly, goes on the class path; Spring Boot's nested bootJar is not
// used (11.C25, Reproducible builds).
tasks.bootJar { enabled = false }
tasks.jar {
    enabled = true
    archiveClassifier = ""
}
tasks.named("bootStartScripts") { enabled = false }
tasks.named("bootDistZip") { enabled = false }
tasks.named("bootDistTar") { enabled = false }
tasks.withType<AbstractArchiveTask>().configureEach {
    isPreserveFileTimestamps = false
    isReproducibleFileOrder = true
}

// A development run from the source tree: the backend staged by its CMake build, which finds its
// own home beside it at build/dev/stage/share/fusion (S8.01: configuration, never the working
// directory). Another backend with -Pfusion.backend.executable=....
tasks.named<JavaExec>("run") {
    val repository = layout.projectDirectory.dir("..").asFile
    val executable = providers.gradleProperty("fusion.backend.executable")
        .orElse(repository.resolve("build/dev/stage/bin/fusion").path)
    val javafxJars = configurations.runtimeClasspath.get().filter { it.name.startsWith("javafx-") }
    classpath = classpath.filter { !it.name.startsWith("javafx-") }
    jvmArgumentProviders.add(CommandLineArgumentProvider {
        listOf("--module-path", javafxJars.asPath, "--add-modules", "javafx.controls",
               "-Dfusion.backend.executable=" + executable.get())
    })
}

tasks.test {
    useJUnitPlatform()
    maxHeapSize = "512m"
    // The tests read the shipped share/fusion/pipelines/standard.json and FuSION 1's synthetic toy
    // inputs under backend/tests/reference/synthetic/, from the repository.
    val repository = layout.projectDirectory.dir("..").asFile
    jvmArgumentProviders.add(CommandLineArgumentProvider {
        listOf("-Dfusion.test.repository=" + repository.canonicalPath)
    })
    // A synced write takes 40 to 65 ms on the shared workstation's disk; AtomicFilesTest keeps it covered.
    systemProperty("fusion.files.fsync", "false")
    inputs.dir(repository.resolve("share/fusion/pipelines")).withPropertyName("pipelines")
    testLogging {
        events("failed")
        exceptionFormat = org.gradle.api.tasks.testing.logging.TestExceptionFormat.FULL
    }
}
