plugins { id("com.android.application") }

// Le jeu web (god-dome-prototype) est copié dans les assets de l'APK : il tourne hors connexion.
val copyGame by tasks.registering(Sync::class) {
    from("../../god-dome-prototype") {
        include("index.html", "src/**", "lib/**", "models/**")
        exclude("models/README.md")
    }
    into(layout.buildDirectory.dir("generated/gameAssets/game"))
}

android {
    namespace = "com.superessence.goddome"
    compileSdk = 35
    defaultConfig {
        applicationId = "com.superessence.goddome"
        minSdk = 26
        targetSdk = 35
        versionCode = 6
        versionName = "0.6.0"
    }
    // Signature : clé fournie par variables d'environnement (jamais dans le dépôt) ; sinon clé de debug.
    val ks = System.getenv("GODDOME_KEYSTORE")
    signingConfigs {
        if (ks != null) create("release") {
            storeFile = file(ks)
            storePassword = System.getenv("GODDOME_STORE_PASS")
            keyAlias = System.getenv("GODDOME_KEY_ALIAS") ?: "goddome"
            keyPassword = System.getenv("GODDOME_KEY_PASS")
        }
    }
    buildTypes {
        release {
            isMinifyEnabled = false
            signingConfig = if (ks != null) signingConfigs.getByName("release") else signingConfigs.getByName("debug")
        }
    }
    sourceSets["main"].assets.srcDir(layout.buildDirectory.dir("generated/gameAssets"))
    compileOptions { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 }
    androidResources { noCompress += listOf("js", "glb", "html") }
}
tasks.named("preBuild") { dependsOn(copyGame) }

dependencies { implementation("androidx.webkit:webkit:1.12.1") }
