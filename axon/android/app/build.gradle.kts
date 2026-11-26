plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

val relayUrl: String = (project.findProperty("axonRelay") as String?) ?: System.getenv("AXON_RELAY_URL") ?: ""
val buildNumber: Int = (System.getenv("GITHUB_RUN_NUMBER") ?: project.findProperty("axonBuild") as String? ?: "1").toInt()

android {
    namespace = "com.superessence.axon"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.superessence.axon"
        minSdk = 26
        targetSdk = 35
        versionCode = buildNumber
        versionName = "1.0.$buildNumber"
        buildConfigField("String", "RELAY_URL", "\"$relayUrl\"")
    }

    signingConfigs {
        create("axon") {
            storeFile = file("../keystore/axon.jks")
            storePassword = System.getenv("AXON_STORE_PASSWORD") ?: "superessence"
            keyAlias = "axon"
            keyPassword = System.getenv("AXON_KEY_PASSWORD") ?: "superessence"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            signingConfig = signingConfigs.getByName("axon")
        }
        debug {
            signingConfig = signingConfigs.getByName("axon")
        }
    }
    buildFeatures { buildConfig = true }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions { jvmTarget = "17" }
}

dependencies {
    implementation("com.squareup.okhttp3:okhttp:4.12.0")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.10.1")
}
