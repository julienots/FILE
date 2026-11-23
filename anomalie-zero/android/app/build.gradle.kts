plugins { id("com.android.application") }
android {
    namespace = "com.anomalie.zero"
    compileSdk = 35
    defaultConfig {
        applicationId = "com.anomalie.zero"
        minSdk = 24
        targetSdk = 35
        versionCode = 4
        versionName = "1.3"
    }
    buildTypes {
        release {
            isMinifyEnabled = false
            // Signé avec la clé de debug : installable directement (hors Play Store)
            signingConfig = signingConfigs.getByName("debug")
        }
    }
    compileOptions { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 }
    androidResources { noCompress += listOf("json", "js", "html") }
}
dependencies { implementation("androidx.webkit:webkit:1.12.1") }
