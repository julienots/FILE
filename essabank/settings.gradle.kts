pluginManagement {
    repositories {
        google()
        // Miroir Google de Maven Central (évite les limitations de débit de repo1)
        maven("https://maven-central.storage-download.googleapis.com/maven2/")
        mavenCentral()
        gradlePluginPortal()
    }
}
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        maven("https://maven-central.storage-download.googleapis.com/maven2/")
        mavenCentral()
    }
}
rootProject.name = "EssaBank"

// :core = moteur financier pur Kotlin (aucune dépendance Android), testable sur JVM.
include(":core")
// :app = application Android (Jetpack Compose). `-PcoreOnly` permet de travailler sur le moteur seul.
if (!providers.gradleProperty("coreOnly").isPresent) include(":app")
