// Vérification de typage de l'interface ESSABANK SANS SDK Android :
// compile app/src/main/java contre Compose Multiplatform (mêmes API que Compose Android 1.8 / Material 3 1.3),
// le framework Android (android-all de Robolectric, Maven Central) et des stubs de signatures AndroidX.
// Usage : ../../gradlew -p tools/ui-typecheck compileKotlin   (après ./gradlew -PcoreOnly :core:jar)
plugins { kotlin("jvm") version "2.1.21"; id("org.jetbrains.kotlin.plugin.compose") version "2.1.21" }
kotlin { compilerOptions { jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17); freeCompilerArgs.add("-Xjdk-release=17") } }
java { targetCompatibility = JavaVersion.VERSION_17; sourceCompatibility = JavaVersion.VERSION_17 }
sourceSets["main"].kotlin.srcDirs("../../app/src/main/java", "stubs")
dependencies {
    val cmp = "1.8.2"
    implementation("org.jetbrains.compose.runtime:runtime-desktop:$cmp")
    implementation("org.jetbrains.compose.ui:ui-desktop:$cmp")
    implementation("org.jetbrains.compose.foundation:foundation-desktop:$cmp")
    implementation("org.jetbrains.compose.animation:animation-desktop:$cmp")
    implementation("org.jetbrains.compose.material3:material3-desktop:$cmp")
    implementation("org.jetbrains.compose.material:material-icons-extended-desktop:1.7.3")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.10.2")
    implementation("org.jetbrains.kotlinx:kotlinx-serialization-json:1.8.1")
    implementation(files("../../core/build/libs/core.jar"))
    compileOnly("org.robolectric:android-all:15-robolectric-12650502")
}
configurations.all {
    exclude(group = "androidx.lifecycle"); exclude(group = "androidx.savedstate"); exclude(group = "androidx.annotation")
    exclude(group = "androidx.collection"); exclude(group = "org.jetbrains.androidx.lifecycle"); exclude(group = "org.jetbrains.androidx.savedstate")
}
