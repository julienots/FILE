pluginManagement {
    repositories {
        google()
        // Miroir Google de Maven Central (plus fiable que repo1 en cas de limitation de débit)
        maven("https://maven-central.storage-download.googleapis.com/maven2/")
        maven("https://repo1.maven.org/maven2/")
        gradlePluginPortal()
    }
}
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        // Miroir Google de Maven Central (plus fiable que repo1 en cas de limitation de débit)
        maven("https://maven-central.storage-download.googleapis.com/maven2/")
        maven("https://repo1.maven.org/maven2/")
    }
}
rootProject.name = "Noctea"
include(":app")
