plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
    id("org.jetbrains.kotlin.plugin.serialization")
}

android {
    namespace = "com.superessence.bodymotion"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.superessence.bodymotion"
        minSdk = 26
        targetSdk = 35
        versionCode = 4
        versionName = "1.0.3"
        ndk { abiFilters += listOf("arm64-v8a", "armeabi-v7a") }
    }

    signingConfigs {
        create("release") {
            storeFile = rootProject.file(System.getenv("BM_KEYSTORE") ?: "keystore/bodymotion-release.jks")
            storePassword = System.getenv("BM_KEYSTORE_PASSWORD") ?: "bodymotion-superessence"
            keyAlias = System.getenv("BM_KEY_ALIAS") ?: "bodymotion"
            keyPassword = System.getenv("BM_KEY_PASSWORD") ?: "bodymotion-superessence"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            signingConfig = signingConfigs.getByName("release")
        }
        debug { applicationIdSuffix = ".debug" }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions { jvmTarget = "17" }
    buildFeatures { compose = true; buildConfig = true }
    androidResources { noCompress += listOf("task", "glb", "filamat") }
    packaging {
        resources.excludes += "/META-INF/{AL2.0,LGPL2.1}"
        jniLibs.useLegacyPackaging = false
    }
    testOptions {
        unitTests.isReturnDefaultValues = true
        unitTests.isIncludeAndroidResources = true
        unitTests.all {
            it.maxHeapSize = "3g"
            it.systemProperty("bm.screenshots", (project.findProperty("screenshots") ?: "false").toString())
            it.systemProperty("robolectric.dependency.repo.url", "https://maven-central.storage-download.googleapis.com/maven2")
        }
    }
}

dependencies {
    val composeBom = platform("androidx.compose:compose-bom:2025.06.01")
    implementation(composeBom)
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.ui:ui-graphics")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.material:material-icons-extended")
    implementation("androidx.compose.animation:animation")
    implementation("androidx.activity:activity-compose:1.10.1")
    implementation("androidx.navigation:navigation-compose:2.9.0")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.9.1")
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.9.1")
    implementation("androidx.core:core-ktx:1.16.0")
    implementation("org.jetbrains.kotlinx:kotlinx-serialization-json:1.8.1")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.10.2")

    val camerax = "1.4.2"
    implementation("androidx.camera:camera-core:$camerax")
    implementation("androidx.camera:camera-camera2:$camerax")
    implementation("androidx.camera:camera-lifecycle:$camerax")
    implementation("androidx.camera:camera-view:$camerax")

    implementation("com.google.mediapipe:tasks-vision:0.10.35")

    val filament = "1.75.1"
    implementation("com.google.android.filament:filament-android:$filament")
    implementation("com.google.android.filament:gltfio-android:$filament")
    implementation("com.google.android.filament:filament-utils-android:$filament")

    testImplementation("junit:junit:4.13.2")
    testImplementation(composeBom)
    testImplementation("androidx.compose.ui:ui-test-junit4")
    debugImplementation("androidx.compose.ui:ui-test-manifest")
    testImplementation("org.robolectric:robolectric:4.14.1")
    testImplementation("androidx.test:core-ktx:1.6.1")
}
