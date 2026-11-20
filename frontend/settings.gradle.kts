// FuSION 2.0's frontend: one Gradle project (section 11, 11.C13 and 11.C14).

pluginManagement {
    repositories {
        gradlePluginPortal()
    }
}

plugins {
    // Downloads a JDK 21 only when none is installed (11.C13).
    id("org.gradle.toolchains.foojay-resolver-convention") version "1.0.0"
}

dependencyResolutionManagement {
    repositoriesMode = RepositoriesMode.FAIL_ON_PROJECT_REPOS
    repositories {
        mavenCentral()
    }
}

rootProject.name = "fusion-frontend"
